"""Independent wire and CSV checks for this candidate; no simulator control."""
from pathlib import Path
import importlib.util,json,re,statistics,hashlib,bisect

work=Path(__file__).resolve().parent
repo=work.parents[3]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m);return m
cap=module('capture',repo/'tools/inspect_capture.py')
csv=module('csvcheck',repo/'tools/analyze_export.py')
folder=work/'suite'
channels,end=cap.load(folder/'suite.vcd')

def decode(trace):
    times,values=trace;eligible=0;bit=1e9/115200;frames=[]
    for i,(time,level) in enumerate(zip(times,values)):
        if time<eligible or level or i==0 or values[i-1]!=1:continue
        byte=sum(cap.value(trace,time+(1.5+b)*bit)<<b for b in range(8))
        frames.append((time,byte,cap.value(trace,time+9.5*bit)))
        eligible=time+9.7*bit
    lines=[];data=bytearray();start=None
    for time,byte,stop in frames:
        if start is None:start=time
        data.append(byte)
        if byte==10:
            lines.append({'start_ns':start,'end_ns':round(time+10*bit),'text':data.decode(errors='replace').rstrip()})
            data.clear();start=None
    return lines,sum(not stop for t,b,stop in frames),len(frames)
tx,tbad,tn=decode(channels['UART_TX']);rx,rbad,rn=decode(channels['UART_RX'])
(folder/'wire.json').write_text(json.dumps({'tx_bad_stop':tbad,'rx_bad_stop':rbad,'tx_bytes':tn,'rx_bytes':rn,'tx_lines':tx,'rx_lines':rx},indent=2)+'\n')
raw=(folder/'uart.raw').read_text(errors='replace')
snapshots=csv.snapshots(raw)
(folder/'export-check.json').write_text(json.dumps(snapshots,indent=2)+'\n')
exports=[];opened=None
for line in tx:
    if line['text'].startswith('export_begin'):opened=line['start_ns']
    if line['text'].startswith('export_end') and opened is not None:
        exports.append((opened,line['end_ns']));opened=None
point_times=[t for t in channels['POINT'][0] if t>0]
buttons=cap.pulses(channels['BUTTON']);events=[]
for start,stop in buttons:
    at=bisect.bisect_left(point_times,start)
    point=point_times[at] if at<len(point_times) and point_times[at]<stop+50000000 else None
    events.append({'button_ns':start,'point_ns':point,'latency_ms':(point-start)/1e6 if point else None,
                   'during_export':any(a<=start<=b for a,b in exports),
                   'point_during_export':point is not None and any(a<=point<=b for a,b in exports)})
(folder/'events.json').write_text(json.dumps(events,indent=2)+'\n')
def summary(values):
    return {'n':len(values),'min':min(values),'avg':statistics.mean(values),'max':max(values)} if values else {'n':0}
reset=[line for line in tx if line['text'].startswith('metrics_reset')]
metric_start=[line for line in tx if line['text'].startswith('METRIC')]
first_measure=next(line for line in tx if line['text'].startswith('MEASURE'))
s1_start=reset[0]['end_ns'];s1_end=next(line['start_ns'] for line in metric_start if line['start_ns']>s1_start)
periodic={}
for key in ['ADC_MARK','IMU_MARK']:
    pulses=cap.pulses(channels[key]);starts=[a for a,b in pulses if s1_start<a<s1_end]
    periodic[key]={'period_ms':summary([(b-a)/1e6 for a,b in zip(starts,starts[1:])]),
                   'width_us':summary([(b-a)/1000 for a,b in pulses if s1_start<a<s1_end])}
metrics=[line['text'] for line in tx if line['text'].startswith(('MEASURE','METRIC','metrics_reset'))]
uart_rates=[]
for begin,finish in [(118,123),(123,128),(128,133)]:
    lines=[line for line in rx if begin*1e9<=line['start_ns']<finish*1e9 and line['text']=='status']
    uart_rates.append({'vcd_window_s':[begin,finish],'status_lines':len(lines),'average_commands_per_s':len(lines)/(finish-begin)})
errors=[line for line in tx if line['text'].startswith('reply=error')]
result={'elf_sha256':hashlib.sha256((folder/'firmware.elf').read_bytes()).hexdigest(),
        'vcd_sha256':hashlib.sha256((folder/'suite.vcd').read_bytes()).hexdigest(),'vcd_duration_s':end/1e9,
        'S1':{'wire_window_s':[s1_start/1e9,s1_end/1e9],'duration_s':(s1_end-s1_start)/1e9,'periodic':periodic,'measure':first_measure['text']},
        'S3':{'buttons':len(buttons),'points':len(point_times),'unmatched':sum(e['point_ns'] is None for e in events),
              'latency_ms':summary([e['latency_ms'] for e in events if e['latency_ms'] is not None]),
              'buttons_during_export':sum(e['during_export'] for e in events),
              'points_during_export':sum(e['point_during_export'] for e in events),
              'completed_exports_wire':len(exports),'completed_exports_crc':len(snapshots),'last_snapshot':snapshots[-1]},
        'UART':{'tx_bad_stop':tbad,'rx_bad_stop':rbad,'wire_status_rates':uart_rates,'error_responses':errors},
        'metrics':metrics,'imu_probe_lines':re.findall(r'probe=imu[^\r\n]*',raw),
        'limits':'Wokwi only. UART command rate windows use VCD seconds; generator target rates were host-time rates. Repeated status replies include unsolicited status, so no unique latency pairing is claimed. Internal microsecond metrics have 1 ms resolution. Raw logs/captures retained; flash and loaded IMU failures are reported, not erased.'}
(folder/'analysis.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:result[k] for k in ['S1','S3','UART']},indent=2))
