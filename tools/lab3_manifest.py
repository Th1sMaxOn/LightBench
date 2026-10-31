"""Bind current build inputs and actual headless outputs. Never invent runtime values."""
from pathlib import Path
import datetime
import hashlib
import json
import re
import subprocess

root=Path(__file__).resolve().parents[1]
out=root/'docs/metrics/lab3'
path=out/'verification.json'
previous=json.loads(path.read_text()) if path.exists() else {}
sources=[]
for folder in ['firmware','tests','tools']:
    for p in (root/folder).rglob('*'):
        if not p.is_file() or any(x in p.relative_to(root).parts for x in ['Drivers','build','__pycache__']):continue
        if p.suffix in {'.c','.h','.s','.ld','.py'} or p.name=='Makefile':sources.append(p)
sources += [root/'Makefile',root/'diagram.json',root/'diagram-lab3.json',root/'wokwi.toml']
digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
log=(out/'build-2.log').read_text()
flash=int(re.search(r'FLASH:\s+(\d+) B',log)[1])
ram=int(re.search(r'RAM:\s+(\d+) B',log)[1])
frames=[int(m[1]) for m in re.finditer(r'\t(\d+)\t', (out/'stack-frames.su').read_text())]
result={'date':datetime.datetime.now().astimezone().isoformat(),
        'head':subprocess.check_output(['git','-C',str(root),'rev-parse','HEAD'],text=True).strip(),
        'branch':subprocess.check_output(['git','-C',str(root),'branch','--show-current'],text=True).strip(),
        'build_id':(root/'firmware/build/build_identity.h').read_text().strip(),
        'builds_identical':json.loads((out/'build-1.sha256.json').read_text())==json.loads((out/'build-2.sha256.json').read_text()),
        'source_sha256':{str(p.relative_to(root)):digest(p) for p in sorted(set(sources))},
        'evidence_sha256':{str(p.relative_to(out)):digest(p) for p in out.rglob('*') if p.is_file() and p!=path},
        'measured':{'flash_bytes':flash,'ram_linker_bytes':ram,'flash_region_bytes':28672,'ram_bytes':12288,
                    'heap_reserve':512,'stack_reserve':2048,'max_single_static_frame':max(frames)},
        'runtime_stack':previous.get('runtime_stack'), 'heap_runtime':previous.get('heap_runtime'),
        'S1':previous.get('S1','prepared_not_run'),'S2_hour':previous.get('S2_hour','prepared_not_run'),
        'S3':previous.get('S3','prepared_not_run'),'power_loss_MCU':previous.get('power_loss_MCU','not_run'),
        'timer_model':previous.get('timer_model','not_verified'),'flash_model':previous.get('flash_model','not_verified'),
        'host_tests':'See actual test-macos.log and test-linux.log. No MCU timing is inferred.',
        'coverage':json.loads((out/'coverage.json').read_text())['totals']}
path.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
print(result['build_id'],result['measured'],'identical=',result['builds_identical'])
