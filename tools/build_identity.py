"""Identify all build inputs without depending on a commit or dirty flag alone."""
from pathlib import Path
import hashlib
import subprocess

root = Path(__file__).resolve().parents[1]
paths = sorted(p for p in (root / 'firmware').rglob('*')
               if p.is_file() and p.suffix in {'.c', '.h', '.s', '.ld'}
               and 'build' not in p.relative_to(root / 'firmware').parts)
paths += [root / 'firmware/Makefile', root / 'tools/build_identity.py']
digest = hashlib.sha256()
for path in paths:
    digest.update(str(path.relative_to(root)).encode() + b'\0' + path.read_bytes() + b'\0')
import os
digest.update(b'LAB3_DIAGNOSTIC='+os.environ.get('LAB3_DIAGNOSTIC','').encode()+b'\0')
base = os.environ.get('LIGHTBENCH_BASE')
if not base:
    base = subprocess.check_output(['git', '-C', str(root), 'rev-parse', '--short=7', 'HEAD'], text=True).strip()
print(f'#define BUILD_ID "{base}-{digest.hexdigest()[:12]}"')
