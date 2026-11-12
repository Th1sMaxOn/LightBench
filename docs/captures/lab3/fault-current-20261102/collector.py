import json,re,time
from pathlib import Path
import serial
p=Path(__file__).resolve().parent
assert not (p/'uart.raw').exists()
started=time.monotonic(); channel=None; reason='connection_timeout'; commands=[]
while time.monotonic()-started<90:
 try:channel=serial.serial_for_url('rfc2217://localhost:4563',baudrate=115200,timeout=.05);break
 except (OSError,serial.SerialException):time.sleep(.2)
if channel:
 reason='wall_timeout'; next_cmd=0; data=b''; running=False; first=None
 with channel,(p/'uart.raw').open('wb') as f:
  while time.monotonic()-started<90:
   now=time.monotonic()
   if now>=next_cmd:
    command=b'status\n'
    if not running and re.search(rb'status ms=\d+[^\r\n]*imu=1',data): command=b'run\n';running=True
    channel.write(command);commands.append({'host_elapsed':now-started,'command':command.decode().strip()});next_cmd=now+.7
   chunk=channel.read(channel.in_waiting or 1)
   if chunk:
    f.write(chunk);f.flush();data=(data+chunk)[-32768:];ms=re.findall(rb'status ms=(\d+)',data)
    if ms:
     first=int(ms[0]) if first is None else first
     if int(ms[-1])>=22000:reason='mcu_22s';break
(p/'session.json').write_text(json.dumps({'reason':reason,'elapsed_host':time.monotonic()-started,'commands':commands},indent=2)+'\n')
(p/'DONE').write_text(reason+'\n');print(reason,flush=True)
