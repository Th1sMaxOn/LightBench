"""Extract metrics from actual MCU logs, keeping missing measurements explicit."""
from pathlib import Path
import argparse
import hashlib
import json
import re

parser = argparse.ArgumentParser()
parser.add_argument('log', type=Path)
parser.add_argument('output', type=Path)
args = parser.parse_args()
text = args.log.read_text(errors='replace')
metrics = []
measurements = []
blocks = []
pending = []
for line in text.splitlines():
    if line.startswith('METRIC ') or line.startswith('MEASURE '):
        fields = dict(re.findall(r'(\w+)=([^\s]+)', line))
        row = {key: int(value) if value.isdigit() else value for key,value in fields.items()}
        if line.startswith('METRIC '):
            metrics.append(row)
            pending.append(row)
        else:
            if row.get('elapsed_us', 0) and row.get('resolution_us') != 1000:
                row['task_occupancy_percent'] = 100*row['busy_us']/row['elapsed_us']
            measurements.append(row)
            blocks.append({'metrics':pending,'measurement':row})
            pending=[]
points = [dict((k,int(v)) for k,v in re.findall(r'(\w+)=(\d+)', line))
          for line in text.splitlines() if line.startswith('point ')]
errors=[]
for number,block in enumerate(blocks,1):
    measure=block['measurement']
    elapsed=measure.get('elapsed_us',0)
    for metric in block['metrics']:
        if metric.get('n',0) and not (0 <= metric['min_us'] <= metric['avg_us'] <= metric['max_us'] <= elapsed):
            errors.append(f"Block {number}, {metric.get('name')}: min/avg/max exceeds its observation interval or is unordered.")
    if measure.get('busy_us',0)>elapsed:
        errors.append(f'Block {number}: foreground busy_us exceeds elapsed_us.')
    period=measure.get('period_ms',0)*1000
    if period and elapsed:
        bound=(elapsed+period-1)//period+2
        for name in ['missed_adc','missed_imu']:
            if measure.get(name,0)>bound:
                errors.append(f'Block {number}: {name} exceeds the number of possible releases in this interval.')
    if len(block['metrics'])!=12:
        errors.append(f'Block {number}: incomplete metric block ({len(block["metrics"])}/12).')
if pending: errors.append('Log ends with a metric block lacking MEASURE.')
if not metrics or not measurements: errors.append('Missing complete MCU metrics.')
result = {'source': str(args.log.resolve()), 'source_sha256':hashlib.sha256(args.log.read_bytes()).hexdigest(),
          'valid':not errors,'validation_errors':errors,
          'validation_scope':'Internal consistency only; independent VCD clock validation is still required.',
          'metrics': metrics, 'measurements': measurements,
          'point_count': len(points),
          'polled_button_latency_ms': [p['ms']-p['edge_ms'] for p in points],
          'limitations': ['Reaction starts at polled candidate, not the physical edge.',
                         'Task occupancy excludes spin/eligibility checks and IRQ outside tasks.',
                         'SysTick measurements have 1 ms resolution and can lose time during flash stalls; VCD is required for shorter durations.',
                         'A sum of quantized task durations is not a reliable CPU occupancy measurement.',
                         'Flash model needs independent validation; no physical timing claim.']}
if errors:
    for measurement in measurements:
        measurement.pop('task_occupancy_percent',None)
args.output.write_text(json.dumps(result, indent=2)+'\n')
if errors:
    raise SystemExit('Invalid or incomplete MCU metrics. See validation_errors; no passing result or occupancy is reported.')
