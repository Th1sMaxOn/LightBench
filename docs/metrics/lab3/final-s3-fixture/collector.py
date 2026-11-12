import json,re,time
from pathlib import Path
import serial

folder=Path(__file__).resolve().parent
assert not (folder/'uart.raw').exists()
began=time.monotonic(); channel=None; commands=[]; marks=[]
while time.monotonic()-began<100:
    try:
        channel=serial.serial_for_url('rfc2217://localhost:4564',baudrate=115200,timeout=.005)
        break
    except (OSError,serial.SerialException): time.sleep(.2)
reason='connection_timeout'; latest=0
if channel:
    reason='wall_timeout'; pending=b''; done=set(); export_open=False
    next_export=next_load=0.; last_export_count=0
    def send(text,stage):
        channel.write((text+'\n').encode())
        commands.append({'host_elapsed':time.monotonic()-began,'latest_mcu_ms':latest,'stage':stage,'command':text})
    def once(key,text):
        if key not in done:
            done.add(key);marks.append({'stage':key,'latest_mcu_ms':latest});send(text,key)
    with channel,(folder/'uart.raw').open('wb') as raw:
        send('status','boot')
        while time.monotonic()-began<240:
            chunk=channel.read(channel.in_waiting or 1)
            if chunk:
                raw.write(chunk);raw.flush();pending+=chunk
                while b'\n' in pending:
                    line,pending=pending.split(b'\n',1)
                    m=re.search(rb'status ms=(\d+)',line)
                    if m:latest=int(m[1])
                    if b'export_begin ' in line:export_open=True
                    m=re.search(rb'export_end count=(\d+)',line)
                    if m:export_open=False;last_export_count=int(m[1])
            now=time.monotonic()
            if latest>=1000:once('S1-start','metrics reset\nrun')
            if latest>=12000:once('ADC-end','stop\nstatus')
            if latest>=67000:once('S1-end','metrics')
            if latest>=69000:once('S3-start','metrics reset\nrun')
            if 72000<=latest<113500:
                if not export_open and now>=next_export:
                    send('export','S3');export_open=True;next_export=now+(.03 if last_export_count else .15)
                if now>=next_load:send('status','S3');next_load=now+.15
            if latest>=114000:once('S3-end','stop\nmetrics\nexport')
            rate=5 if 118000<=latest<123000 else 20 if 123000<=latest<128000 else 80 if 128000<=latest<133000 else 0
            if rate and now>=next_load:
                send('status',f'UART-{rate}-host-Hz');next_load=now+1/rate
            if latest>=134000:once('UART-end','metrics')
            if latest>=136000:once('UART-long-line','x'*160+'\nstatus')
            if latest>=138000:once('final','status\nmetrics')
            if latest>=141000:reason='mcu_141s';break
(folder/'session.json').write_text(json.dumps({'reason':reason,'host_elapsed':time.monotonic()-began,'latest_mcu_ms':latest,'marks':marks,'commands':commands},indent=2)+'\n')
(folder/'DONE').write_text(reason+'\n');print(reason,latest,flush=True)
