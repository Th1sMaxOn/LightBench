"""Exercise LightBench's project gate against a disposable project copy."""
import importlib.util
from pathlib import Path
import shutil
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('project_gate', ROOT / 'tools/check_project.py')
gate = importlib.util.module_from_spec(spec)
spec.loader.exec_module(gate)


class ProjectGateTest(unittest.TestCase):
    def test_rejects_missing_domain_and_hardware_dependency(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            firmware = root / 'firmware'
            for folder in ('App', 'Devices', 'Platform', 'Core/Src'):
                (firmware / folder).mkdir(parents=True, exist_ok=True)
            for folder in ('App', 'Devices'):
                for p in (ROOT / 'firmware' / folder).glob('*.[ch]'):
                    shutil.copy2(p, firmware / folder / p.name)
            for relative in ('Platform/runtime.c', 'Core/Src/main.c', 'Makefile'):
                shutil.copy2(ROOT / 'firmware' / relative, firmware / relative)
            shutil.copytree(ROOT / 'tests', root / 'tests', ignore=shutil.ignore_patterns('build', '__pycache__'))
            with patch.multiple(gate, ROOT=root, FIRMWARE=firmware,
                                DOMAIN=firmware / 'App', DRIVERS=firmware / 'Devices'):
                gate.verify_dependencies()
                gate.verify_build_inventory()
                bench = firmware / 'App/bench.c'
                original = bench.read_text()
                bench.unlink()
                with self.assertRaisesRegex(ValueError, 'missing LightBench module'):
                    gate.verify_dependencies()
                bench.write_text(original + '\nvoid forbidden(void) { HAL_Delay(1); }\n')
                with self.assertRaisesRegex(ValueError, 'hardware in portable module'):
                    gate.verify_dependencies()
                bench.write_text(original)
                makefile = firmware / 'Makefile'
                makefile.write_text(makefile.read_text().replace('App/records.c', 'App/missing.c'))
                with self.assertRaisesRegex(ValueError, 'records: firmware'):
                    gate.verify_build_inventory()


if __name__ == '__main__':
    unittest.main()
