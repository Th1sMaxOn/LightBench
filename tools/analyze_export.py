#!/usr/bin/env python3
"""Check a complete LightBench LB1 snapshot independently of the firmware."""
import argparse
import json
import re
import struct
import zlib
from pathlib import Path


def snapshots(text):
    header = "seq,series,mark_ms,light_ms,pose_ms,raw,angle_cdeg,flags,reserved"
    active = None
    results = []
    for line in text.splitlines():
        begin = re.fullmatch(r"export_begin count=(\d+) overwritten=(\d+) format=LB1_le_28 crc=IEEE", line)
        if begin:
            if active is not None:
                raise ValueError("Nested or unfinished snapshot")
            active = {"count": int(begin[1]), "overwritten": int(begin[2]), "rows": [], "header": False}
            if active["count"] > 64:
                raise ValueError("Count exceeds capacity")
        elif active is not None and line == header:
            active["header"] = True
        elif active is not None and line.startswith("export_end"):
            end = re.fullmatch(r"export_end count=(\d+) crc32=([0-9a-fA-F]{8})", line)
            if not end or not active["header"]:
                raise ValueError("Malformed trailer or missing header")
            rows = active["rows"]
            if len(rows) != active["count"] or int(end[1]) != len(rows):
                raise ValueError("Incomplete row count")
            data = b"".join(struct.pack("<IIIIIHHHH", *row) for row in rows)
            crc = zlib.crc32(data)
            if crc != int(end[2], 16):
                raise ValueError("CRC mismatch")
            results.append({"rows": len(rows), "overwritten": active["overwritten"],
                            "rejected_pairs": sum(row[7] != 0 for row in rows),
                            "crc32": f"{crc:08x}", "series": sorted({row[1] for row in rows})})
            active = None
        elif active is not None and re.match(r"^\d+,", line):
            row = [int(value) for value in line.split(",")]
            if len(row) != 9 or any(not 0 <= x <= 0xffffffff for x in row[:5]):
                raise ValueError("Invalid timestamp or ID fields")
            if any(not 0 <= x <= 65535 for x in row[5:]) or row[8] != 0 or row[7] & ~63:
                raise ValueError("Invalid point fields")
            if row[5] > 4095 and row[5] != 65535:
                raise ValueError("Raw outside ADC range")
            if row[6] > 18000 and row[6] != 65535:
                raise ValueError("Angle outside range")
            if active["rows"] and row[0] <= active["rows"][-1][0]:
                raise ValueError("Sequence is not increasing")
            active["rows"].append(row)
        # Interleaved status/point replies are allowed during export.
    if active is not None or not results:
        raise ValueError("No complete snapshot")
    return results


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("uart_log", type=Path)
    args = parser.parse_args()
    print(json.dumps(snapshots(args.uart_log.read_text()), indent=2))
