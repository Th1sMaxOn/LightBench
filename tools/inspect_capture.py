#!/usr/bin/env python3
"""Read this stand's 1 ns scalar VCD, decode bytes and select review windows."""
from array import array
from bisect import bisect_right
import argparse
import gzip
import hashlib
import heapq
import json
from pathlib import Path
import re
import statistics


def load(path):
    channels, symbols, now = {}, {}, 0
    opener = gzip.open if path.suffix == ".gz" else open
    with opener(path, "rt") as stream:
        for line in stream:
            match = re.match(r"\$var wire 1 (\S+) (\S+)", line)
            if match:
                symbol, name = match.groups()
                symbols[symbol] = name
                channels[name] = (array("Q"), array("B"))
            if "$enddefinitions" in line:
                break
        for line in stream:
            line = line.strip()
            if line.startswith("#"):
                now = int(line[1:])
            elif len(line) > 1 and line[0] in "01" and line[1:] in symbols:
                times, values = channels[symbols[line[1:]]]
                value = int(line[0])
                if times and times[-1] == now:
                    values[-1] = value
                elif not values or values[-1] != value:
                    times.append(now); values.append(value)
    return channels, now


def value(trace, time):
    index = bisect_right(trace[0], time) - 1
    return trace[1][index] if index >= 0 else 0


def transactions(channels):
    scl, sda = channels["SCL"], channels["SDA"]
    events = heapq.merge(((t, 0, v) for t, v in zip(*scl)), ((t, 1, v) for t, v in zip(*sda)))
    level = [1, 1]; active = None; bits = []; result = []
    for time, channel, state in events:
        previous = level[channel]; level[channel] = state
        if channel == 1 and level[0] and state != previous:
            if not state:
                active = {"start_ns": time, "bytes": [], "ack": [], "bit_times_ns": []}; bits = []
            elif active is not None:
                active["stop_ns"] = time; result.append(active); active = None
        elif channel == 0 and not previous and state and active is not None:
            bits.append(level[1]); active["bit_times_ns"].append(time)
            if len(bits) == 9:
                byte = 0
                for bit in bits[:8]: byte = (byte << 1) | bit
                active["bytes"].append(byte); active["ack"].append(bits[8]); bits = []
    return result


def pulses(trace):
    result = []; opened = None
    for time, state in zip(*trace):
        if state: opened = time
        elif opened is not None: result.append((opened, time)); opened = None
    return result


def timing(trace):
    spans = pulses(trace)
    periods = [(b[0]-a[0])/1000 for a,b in zip(spans,spans[1:])]
    return {"pulses": len(spans), "width_us_median": statistics.median((b-a)/1000 for a,b in spans),
            "period_us_min": min(periods), "period_us_median": statistics.median(periods),
            "period_us_max": max(periods), "intervals_over_40ms": sum(x>40000 for x in periods)}


def clip(channels, start, end, path):
    names = list(channels); symbols = {n: chr(33+i) for i,n in enumerate(names)}
    with path.open("w") as out:
        out.write("$version LightBench lossless time window $end\n$timescale 1ns $end\n$scope module logic $end\n")
        for name in names: out.write(f"$var wire 1 {symbols[name]} {name} $end\n")
        out.write("$upscope $end\n$enddefinitions $end\n#0\n")
        for name in names: out.write(f"{value(channels[name],start)}{symbols[name]}\n")
        # Each slice binds its own channel name and timestamps.
        streams = [iter([(t,name,v) for t,v in zip(*channels[name]) if start < t <= end]) for name in names]
        for time,name,state in heapq.merge(*streams):
            out.write(f"#{time-start}\n{state}{symbols[name]}\n")


def main():
    parser=argparse.ArgumentParser();parser.add_argument("vcd",type=Path);parser.add_argument("output",type=Path)
    args=parser.parse_args();args.output.mkdir(parents=True,exist_ok=True)
    channels,end=load(args.vcd);tx=transactions(channels)
    identity=next(t for t in tx if t["bytes"][:2]==[0xd1,0x68])
    burst=next(t for t in tx if len(t["bytes"])==15 and t["bytes"][0]==0xd1 and t["start_ns"]>2000000000)
    nack=next(t for t in tx if t["bytes"]==[0xd2] and t["ack"]==[1])
    point_edges=[t for t in channels["POINT"][0] if t>0]
    button=pulses(channels["BUTTON"])
    report={"duration_s":end/1e9,"source_sha256":hashlib.sha256(args.vcd.read_bytes()).hexdigest(),
            "i2c_transactions":len(tx),"identity":identity,"burst":burst,"nack":nack,
            "adc":timing(channels["ADC_MARK"]),"imu":timing(channels["IMU_MARK"]),
            "points":len(point_edges),"button_pulses":len(button)}
    first_scl=pulses(channels["SCL"])
    periods=[(b[0]-a[0]) for a,b in zip(first_scl,first_scl[1:]) if 8000 < b[0]-a[0] < 12000]
    report["scl_period_ns_median"]=statistics.median(periods)
    groups=[]
    for start,stop in button:
        if not groups or start-groups[-1][1]>30000000:
            groups.append([start,stop])
        else: groups[-1][1]=stop
    report["button_press_groups"]=len(groups)
    report["button_to_point_ms"]=[(p-start)/1e6 for start,stop in groups for p in point_edges if start<=p<=stop]
    uart_times=channels["UART_TX"][0]
    report["uart_bit_us_median"]=statistics.median((b-a)/1000 for a,b in zip(uart_times,uart_times[1:]) if 7000<b-a<10000)
    windows={"i2c-whoami":(max(0,identity["start_ns"]-500000),identity["stop_ns"]+500000),
             "i2c-burst":(max(0,burst["start_ns"]-500000),burst["stop_ns"]+500000),
             "i2c-nack":(nack["start_ns"]-100000,nack["stop_ns"]+100000),
             "adc-period":(1000000000,1120000000),
             "button-point":(button[0][0]-10000000,button[0][0]+60000000),
             "uart-startup":(0,150000000)}
    for name,(start,stop) in windows.items():
        path=args.output/(name+".vcd");clip(channels,start,stop,path)
        path.with_suffix(".origin.json").write_text(json.dumps({"source_sha256":report["source_sha256"],
            "start_ns":start,"end_ns":stop,"channels":list(channels),"sha256":hashlib.sha256(path.read_bytes()).hexdigest(),
            "sampling":"event transitions at 1 ns; SR conversion downsample=50 gives 20 MHz"},indent=2)+"\n")
    (args.output/"measurements.json").write_text(json.dumps(report,indent=2)+"\n")
    (args.output/"i2c-transactions.json").write_text(json.dumps(tx,indent=2)+"\n")
    print(json.dumps({k:v for k,v in report.items() if k not in ("identity","burst","nack","button_to_point_ms")},indent=2))


if __name__=="__main__": main()
