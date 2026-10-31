"""Prepare repeatable Wokwi scenarios. Generation is not a simulation result."""
from pathlib import Path
import argparse
import json
import random

parser = argparse.ArgumentParser()
parser.add_argument('output', type=Path)
args = parser.parse_args()
args.output.mkdir(parents=True, exist_ok=True)

def delay(ms):
    return {'delay': f'{ms}ms'}

def command(text):
    return {'write-serial': text + '\n'}

def control(part, name, value):
    return {'set-control': {'part-id': part, 'name': name, 'value': value}}

def click():
    return [control('mark', 'pressed', 1), delay(65), control('mark', 'pressed', 0), delay(65)]

def save(name, steps):
    # JSON is a YAML 1.2 subset and retains literal UART newlines unambiguously.
    doc = {'name': f'LightBench LR3 {name}', 'version': 1, 'author': 'LightBench',
           'steps': [{'wait-serial': 'project=LightBench'}, delay(300),
                     command('period 20'), command('metrics reset')] + steps +
                    [command('metrics'), {'wait-serial': 'MEASURE'}, delay(1000)]}
    (args.output / f'{name}.yaml').write_text(json.dumps(doc, indent=2) + '\n')

save('S1', [delay(65000)])
s2 = [command('run')]
for i in range(80):
    s2 += [delay(44870), control('ldr', 'lux', [10, 100, 1000, 50][i % 4])] + click()
    if i % 10 == 0:
        s2 += [command('status')]
s2 += [delay(5000), command('stop'), command('export'), {'wait-serial': 'export_end'}]
save('S2-hour', s2)

rng = random.Random(328019)
s3 = [command('run')]
for i in range(40):
    s3 += [delay(rng.randrange(1400, 1800)), command('export')]
    s3 += [command('status\n' * 8), delay(rng.randrange(1, 70))]
    s3 += click()
s3 += [delay(2000), command('stop')]
save('S3', s3)
save('fault-recovery', [command('run'), delay(2000), command('imuaddr 0x69'),
                       delay(3000), command('imuaddr 0x68'), delay(3000), command('stop')])
sweep = [command('run')]
for period in [20, 10, 5, 2, 1]:
    sweep += [command(f'period {period}'), command('metrics reset'), delay(10000),
              command('metrics'), {'wait-serial': 'MEASURE'}, delay(1000)]
sweep += [command('period 20'), command('stop')]
save('sampling-sweep', sweep)

print('Prepared S1=65 s, S2>=3605 s, S3=40 clicks with export/UART, NACK recovery, sampling sweep')
