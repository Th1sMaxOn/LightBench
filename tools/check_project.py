"""Check LightBench's portable domain, build inventory and vendor integrity."""
from pathlib import Path
import hashlib
import json
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = ROOT / 'firmware'
DOMAIN = FIRMWARE / 'App'
DRIVERS = FIRMWARE / 'Devices'
STANDARD = {'stdbool.h', 'stdint.h', 'stddef.h', 'stdio.h', 'string.h',
            'limits.h', 'math.h', 'stdlib.h'}


def verify_dependencies():
    # The bench owns paired samples; records, protocol and persistence are
    # portable and must not acquire a HAL or board dependency.
    required = {'bench', 'pose', 'records', 'protocol', 'point_store'}
    for name in required:
        for suffix in ('.c', '.h'):
            if not (DOMAIN / (name + suffix)).is_file():
                raise ValueError(f'missing LightBench module: {name}{suffix}')
    dependencies = {
        'bench': {'bench.h', 'pose.h', 'records.h'},
        'point_store': {'point_store.h', 'records.h'},
        'records': {'records.h'}, 'protocol': {'protocol.h'},
        'pose': {'pose.h'}, 'mpu6050': {'mpu6050.h'},
    }
    for folder in (DOMAIN, DRIVERS):
        for source in folder.glob('*.[ch]'):
            text = source.read_text()
            if source.stem not in dependencies:
                raise ValueError(f'unreviewed domain module: {source}')
            for header in re.findall(r'#include\s*[<\"]([^>\"]+)', text):
                if header not in dependencies[source.stem] | STANDARD:
                    raise ValueError(f'{source}: nonportable include {header}')
            if re.search(r'\bHAL_|\bGPIO[A-F]\b|stm32c0xx', text):
                raise ValueError(f'{source}: hardware in portable module')
    if (FIRMWARE / 'Platform/profile.h').exists():
        raise ValueError('unexpected product profile switch')
    for path in (FIRMWARE / 'Platform/runtime.c', FIRMWARE / 'Core/Src/main.c'):
        if 'HAL_Delay(' in path.read_text():
            raise ValueError(f'blocking delay in {path}')


def verify_build_inventory():
    build = (FIRMWARE / 'Makefile').read_text()
    test_build = (ROOT / 'tests/Makefile').read_text()
    for module in ('bench', 'pose', 'records', 'protocol', 'point_store'):
        if f'App/{module}.c' not in build or f'App/{module}.c' not in test_build:
            raise ValueError(f'{module}: firmware or host test inventory missing')
    for source in ('Platform/runtime.c', 'Platform/light_input.c', 'Devices/mpu6050.c'):
        if source not in build:
            raise ValueError(f'firmware inventory missing {source}')
    for suite in ('test_bench', 'test_protocol', 'test_point_store', 'test_runtime', 'test_light_input'):
        if suite not in test_build or not (ROOT / 'tests' / (suite + '.c')).is_file():
            raise ValueError(f'host test missing {suite}')
    if not (ROOT / 'tests/test_export.py').is_file():
        raise ValueError('canonical record export check missing')


def verify_vendor_manifest():
    manifest = json.loads((ROOT / 'docs/vendor-manifest.json').read_text())
    for relative, expected in manifest['files'].items():
        path = FIRMWARE / relative
        if hashlib.sha256(path.read_bytes()).hexdigest() != expected:
            raise ValueError(f'vendor file changed: {path}')


def main():
    verify_dependencies()
    verify_build_inventory()
    verify_vendor_manifest()
    print('LightBench domain boundaries, build/test inventory and vendor hashes passed')


if __name__ == '__main__':
    try:
        main()
    except (OSError, ValueError, KeyError) as error:
        sys.exit(str(error))
