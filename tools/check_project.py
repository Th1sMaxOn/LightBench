from pathlib import Path
import hashlib
import json
import re
root = Path(__file__).resolve().parents[1]
allowed = {'point_store.h', 'bench.h', 'pose.h', 'records.h', 'protocol.h', 'mpu6050.h', 'stdbool.h', 'stdint.h', 'stddef.h', 'stdio.h', 'string.h', 'limits.h', 'math.h', 'stdlib.h'}
for folder in ['App', 'Devices']:
    for source in (root / 'firmware' / folder).glob('*.[ch]'):
        text = source.read_text()
        headers = re.findall(r'#include\s*[<"]([^>"]+)', text)
        assert all(name in allowed for name in headers), (source, headers)
        assert not re.search(r'\bHAL_|\bGPIO[A-F]\b|stm32c0xx', text), source
assert not (root / 'firmware/Platform/profile.h').exists()
for source in [root/'firmware/Platform/runtime.c', root/'firmware/Core/Src/main.c']:
    assert 'HAL_Delay(' not in source.read_text(), source
manifest = json.loads((root / 'docs/vendor-manifest.json').read_text())
for relative, expected in manifest['files'].items():
    path = root / 'firmware' / relative
    assert hashlib.sha256(path.read_bytes()).hexdigest() == expected, path
print('App/Devices dependencies, vendor hashes and absence of profile/delays passed')
