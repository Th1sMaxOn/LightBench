"""Capture LightBench RFC2217 traffic and record host-timed command provenance."""
import argparse
from datetime import datetime
import json
from pathlib import Path
import time

import serial


def load_schedule(path):
    entries = json.loads(path.read_text()) if path else []
    previous = 0.0
    for entry in entries:
        when, command = entry['after_seconds'], entry['text']
        if not isinstance(when, (int, float)) or not previous <= when < float('inf'):
            raise ValueError('schedule times must be finite, nonnegative and ordered')
        if not isinstance(command, str) or not command or '\n' in command or '\r' in command:
            raise ValueError('each scheduled command must be one nonempty line')
        command.encode('ascii')
        previous = when
    return entries


def capture(args):
    schedule = load_schedule(args.schedule)
    if args.seconds <= 0 or args.seconds == float('inf') or args.seconds != args.seconds:
        raise ValueError('duration must be positive and finite')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    session = args.output.with_suffix('.session.json')
    started = datetime.now().astimezone().isoformat()
    session.write_text(json.dumps({'started_at': started, 'port': args.port, 'state': 'recording'}, indent=2) + '\n')
    events, position, next_command = [], 0, 0
    received_count = 0
    reason = 'duration_elapsed'
    epoch = time.monotonic()
    failure = None
    try:
        with serial.serial_for_url(f'rfc2217://localhost:{args.port}', baudrate=115200, timeout=0.05) as uart:
            epoch = time.monotonic()
            with args.output.open('wb') as stream:
                while (elapsed := time.monotonic() - epoch) < args.seconds:
                    if args.stop_file and args.stop_file.exists():
                        reason = 'requested_stop'
                        break
                    commands = []
                    if args.control_file and args.control_file.exists():
                        pending = args.control_file.read_bytes()[position:]
                        complete = pending.rfind(b'\n') + 1
                        commands.extend(raw.decode('ascii') for raw in pending[:complete].splitlines() if raw)
                        position += complete
                    while next_command < len(schedule) and elapsed >= schedule[next_command]['after_seconds']:
                        commands.append(schedule[next_command]['text'])
                        next_command += 1
                    for command in commands:
                        payload = (command + '\n').encode('ascii')
                        if uart.write(payload) != len(payload):
                            raise serial.SerialException('partial command write')
                        events.append({'wall_seconds': elapsed, 'command': command})
                        print(f'{elapsed:.3f}s sent {command}', flush=True)
                    data = uart.read(uart.in_waiting or 1)
                    if data:
                        stream.write(data)
                        stream.flush()
                        received_count += len(data)
    except (serial.SerialException, OSError, UnicodeError) as error:
        failure = error
        reason = str(error)
    finally:
        duration = time.monotonic() - epoch
        session.write_text(json.dumps({
            'started_at': started, 'duration_seconds': duration, 'port': args.port,
            'end_reason': reason, 'commands': events, 'uart_bytes': received_count,
            'clock': 'Host monotonic schedule; UART time_ms comes from the simulated MCU'
        }, indent=2) + '\n')
    if failure:
        raise failure
    print(f'Captured {received_count} bytes', flush=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--port', type=int, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--seconds', type=float, default=65)
    parser.add_argument('--schedule', type=Path)
    parser.add_argument('--control-file', type=Path, help='Append newline-terminated commands during a live run')
    parser.add_argument('--stop-file', type=Path, help='Stop cleanly when this file exists')
    capture(parser.parse_args())


if __name__ == '__main__':
    main()
