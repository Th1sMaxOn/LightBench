"""Wait for this fixture's RFC2217 port, record bytes and stop after 20 s MCU time."""
import datetime,json,re,time
from pathlib import Path
import serial
root=Path(__file__).resolve().parent; limit=time.monotonic()+120; opened=datetime.datetime.now().astimezone().isoformat(); events=[]; reason='connection_timeout'; c=None
while time.monotonic()<limit:
 try:c=serial.serial_for_url('rfc2217://localhost:4531',baudrate=115200,timeout=.05);break
 except (OSError,serial.SerialException):time.sleep(.2)
if c:
 reason='wall_timeout'; epoch=time.monotonic(); next_status=epoch; last=b''; sent_run=False
 with c,(root/'short-fixture.uart.txt').open('wb') as f:
  while time.monotonic()<limit:
   now=time.monotonic()
   if not sent_run:
    c.write(b'run\n');events.append({'wall_s':now-epoch,'command':'run'});sent_run=True
   if now>=next_status:
    c.write(b'status\n');events.append({'wall_s':now-epoch,'command':'status'});next_status=now+0.7
   b=c.read(c.in_waiting or 1)
   if b:
    f.write(b);f.flush();last=(last+b)[-8192:]; values=re.findall(rb'status ms=(\d+)',last)
    if values and int(values[-1])>=20000:reason='mcu_20s';break
 (root/'short-fixture.session.json').write_text(json.dumps({'started_at':opened,'duration_s':time.monotonic()-epoch,'port':4531,'end_reason':reason,'commands':events,'clock':'commands host monotonic; status ms MCU clock'},indent=2)+'\n')
else:(root/'short-fixture.session.json').write_text(json.dumps({'started_at':opened,'port':4531,'end_reason':reason})+'\n')
(root/'COLLECTOR_DONE').write_text(reason+'\n')
print('collector',reason,flush=True)
