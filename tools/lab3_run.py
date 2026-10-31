"""Run the current product through the official Wokwi CLI and bind raw evidence."""
from pathlib import Path
import argparse
import datetime
import hashlib
import json
import os
import subprocess
import time

parser = argparse.ArgumentParser()
parser.add_argument('scenario', type=Path)
parser.add_argument('output', type=Path)
parser.add_argument('--cli', default='wokwi-cli')
parser.add_argument('--seconds', type=int, required=True)
parser.add_argument('--vcd', action='store_true')
parser.add_argument('--diagram', type=Path, default=Path('diagram-lab3.json'))
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
if not os.environ.get('WOKWI_CLI_TOKEN'):
    raise SystemExit('WOKWI_CLI_TOKEN missing. No simulation has been started.')
args.output.mkdir(parents=True, exist_ok=False)
scenario = args.scenario.resolve()
output = args.output.resolve()
diagram=args.diagram.resolve()
files = [diagram, root/'wokwi.toml', root/'firmware/build/firmware.elf',
         root/'firmware/build/firmware.hex', root/'firmware/build/build_identity.h', scenario]
manifest = {'started_at': datetime.datetime.now().astimezone().isoformat(),
            'head': subprocess.check_output(['git', '-C', str(root), 'rev-parse', 'HEAD'], text=True).strip(),
            'input_sha256': {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in files},
            'clock': 'See firmware boot banner; validate MCU time against VCD, not host wall time', 'status': 'running'}
cmd = [args.cli, str(root), '--scenario', str(scenario), '--timeout', str(args.seconds*1000),
       '--serial-log-file', str(output/'uart.txt'), '--diagram-file', str(diagram)]
if args.vcd:
    cmd += ['--vcd-file', str(output/'logic.vcd')]
manifest['command'] = cmd
(output/'origin.json').write_text(json.dumps(manifest, indent=2)+'\n')
started = time.monotonic()
with (output/'cli.log').open('w') as log:
    result = subprocess.run(cmd, stdout=log, stderr=subprocess.STDOUT, cwd=root)
manifest.update(exit_code=result.returncode, host_elapsed_s=time.monotonic()-started,
                status='completed' if result.returncode==0 else 'failed',
                output_sha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in output.iterdir() if p.name!='origin.json'})
(output/'origin.json').write_text(json.dumps(manifest, indent=2)+'\n')
raise SystemExit(result.returncode)
