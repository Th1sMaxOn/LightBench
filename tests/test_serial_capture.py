import argparse
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('serial_capture', Path(__file__).resolve().parents[1] / 'tools/serial_capture.py')
capture = importlib.util.module_from_spec(spec)
spec.loader.exec_module(capture)


class FakeSerial:
    in_waiting = 1
    def __init__(self):
        self.sent = []
        self.reads = 0
    def __enter__(self): return self
    def __exit__(self, *args): pass
    def write(self, value):
        self.sent.append(value)
        return len(value)
    def read(self, size):
        self.reads += 1
        if self.reads == 1: return b'actual\x00uart\n'
        raise capture.serial.SerialException('connection lost')


class CaptureTest(unittest.TestCase):
    def test_disconnect_preserves_bytes_and_session(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            schedule = root / 'schedule.json'
            schedule.write_text(json.dumps([{'after_seconds': 0, 'text': 'status'}]))
            control = root / 'control'
            control.write_bytes(b'run\npartial')
            output = root / 'capture.log'
            args = argparse.Namespace(port=7000, output=output, seconds=10, schedule=schedule,
                                      control_file=control, stop_file=None)
            uart = FakeSerial()
            with patch.object(capture.serial, 'serial_for_url', return_value=uart):
                with self.assertRaises(capture.serial.SerialException): capture.capture(args)
            self.assertEqual(uart.sent, [b'run\n', b'status\n'])
            self.assertEqual(output.read_bytes(), b'actual\x00uart\n')
            session = json.loads(output.with_suffix('.session.json').read_text())
            self.assertEqual(session['uart_bytes'], len(output.read_bytes()))
            self.assertEqual(session['end_reason'], 'connection lost')
            self.assertEqual([item['command'] for item in session['commands']], ['run', 'status'])

    def test_invalid_schedule_leaves_existing_recording_untouched(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            recording = root / 'lightbench.uart'
            recording.write_bytes(b'previous S2 evidence\x00')
            control = root / 'commands'
            control.write_bytes(b'run\n')
            schedule = root / 'commands.json'
            args = argparse.Namespace(port=7000, output=recording, seconds=10,
                                      schedule=schedule, control_file=control, stop_file=None)
            bad_schedules = (
                [{'after_seconds': 2, 'text': 'status'}, {'after_seconds': 1, 'text': 'export'}],
                [{'after_seconds': 0, 'text': 'run\nstop'}],
                [{'after_seconds': -1, 'text': 'status'}],
            )
            for entries in bad_schedules:
                with self.subTest(entries=entries):
                    schedule.write_text(json.dumps(entries))
                    with patch.object(capture.serial, 'serial_for_url') as connect:
                        with self.assertRaises(ValueError):
                            capture.capture(args)
                    connect.assert_not_called()
                    self.assertEqual(recording.read_bytes(), b'previous S2 evidence\x00')
                    self.assertEqual(control.read_bytes(), b'run\n')
                    self.assertFalse(recording.with_suffix('.session.json').exists())



if __name__ == '__main__':
    unittest.main()
