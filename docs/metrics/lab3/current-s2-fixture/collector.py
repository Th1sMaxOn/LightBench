"""Single continuous RFC2217 capture, ending only after one MCU-hour interval."""
import json
from pathlib import Path
import re
import time
import serial

root = Path(__file__).resolve().parent
raw = root / 'uart.raw'
assert not raw.exists(), 'Refusing to overwrite an earlier capture'
start_host = time.monotonic()
commands = []
reason = 'connection_timeout'
first_ms = latest = None
reset_ms = None
final_sent = False
seen_measure = seen_export = False
pending = b''
connection = None
try:
    while time.monotonic() - start_host < 180:
        try:
            connection = serial.serial_for_url('rfc2217://localhost:4557', baudrate=115200, timeout=.1)
            break
        except (OSError, serial.SerialException):
            time.sleep(.25)
    if connection:
        reason = 'host_timeout'
        with connection, raw.open('xb') as output:
            def send(command):
                value = (command + '\n').encode('ascii')
                if connection.write(value) != len(value):
                    raise serial.SerialException('partial command write')
                commands.append({'host_seconds': time.monotonic()-start_host, 'latest_mcu_ms': latest, 'command': command})
            send('status')
            while time.monotonic() - start_host < 15000:
                data = connection.read(connection.in_waiting or 1)
                if not data:
                    continue
                output.write(data)
                output.flush()
                pending += data
                while b'\n' in pending:
                    line, pending = pending.split(b'\n', 1)
                    m = re.search(rb'^status ms=(\d+)', line)
                    if m:
                        latest = int(m.group(1))
                        if first_ms is None:
                            first_ms = latest
                            send('metrics reset')
                            send('run')
                        if reset_ms is not None and not final_sent and latest - reset_ms >= 3605000:
                            send('metrics')
                            send('status')
                            send('export')
                            final_sent = True
                    m = re.search(rb'^metrics_reset us=(\d+)', line)
                    if m and reset_ms is None:
                        reset_ms = int(m.group(1)) // 1000
                    if final_sent and line.startswith(b'MEASURE '): seen_measure = True
                    if final_sent and line.startswith(b'export_end '): seen_export = True
                if final_sent and seen_measure and seen_export:
                    reason = 'mcu_hour_and_final_outputs'
                    break
except Exception as error:
    reason = f'{type(error).__name__}: {error}'
finally:
    summary = {'reason': reason, 'first_mcu_ms': first_ms, 'reset_mcu_ms': reset_ms,
               'last_mcu_ms': latest, 'final_sent': final_sent, 'measure': seen_measure,
               'export': seen_export, 'host_seconds': time.monotonic()-start_host, 'commands': commands}
    (root/'session.json').write_text(json.dumps(summary, indent=2)+'\n')
    (root/'DONE').write_text(reason+'\n')
    print(json.dumps(summary), flush=True)
