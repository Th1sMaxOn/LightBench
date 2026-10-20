"""Golden CRC, interleaved UART, truncated and corrupt snapshot checks."""
import importlib.util
from pathlib import Path
import unittest

spec = importlib.util.spec_from_file_location("export", Path(__file__).parents[1] / "tools/analyze_export.py")
module = importlib.util.module_from_spec(spec)
spec.loader.exec_module(module)
GOOD = """export_begin count=1 overwritten=0 format=LB1_le_28 crc=IEEE
seq,series,mark_ms,light_ms,pose_ms,raw,angle_cdeg,flags,reserved
status ms=99 running=1
1,2,3,4,5,6,7,8,0
export_end count=1 crc32=a51b2176
"""


class ExportTest(unittest.TestCase):
    def test_complete(self):
        self.assertEqual(module.snapshots(GOOD)[0]["rejected_pairs"], 1)
    def test_empty(self):
        empty = GOOD.replace("count=1", "count=0").replace("1,2,3,4,5,6,7,8,0\n", "").replace("a51b2176", "00000000")
        self.assertEqual(module.snapshots(empty)[0]["rows"], 0)
    def test_damaged(self):
        for text in [GOOD.replace("a51b2176", "a51b2177"), GOOD.split("export_end")[0],
                     GOOD.replace("count=1", "count=2"), GOOD.replace(",6,7,8,0", ",5000,7,8,0"),
                     GOOD.replace(",6,7,8,0", ",6,19000,8,0"), GOOD.replace(",6,7,8,0", ",6,7,128,0"),
                     GOOD.replace("seq,series", "sequence,series"), "",
                     GOOD.replace("1,2,3,4,5,6,7,8,0", "1,2,3,4,5,6,7,8,0\n1,2,3,4,5,6,7,8,0")]:
            with self.subTest(text=text), self.assertRaises(ValueError):
                module.snapshots(text)


if __name__ == "__main__":
    unittest.main()
