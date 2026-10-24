#!/usr/bin/env python3
"""Check RAW field encoding and AID coverage, not IEEE conformance or TIM support."""
import argparse
import json
from pathlib import Path


def inspect(path, expected):
    tokens = iter(map(int, Path(path).read_text().split()))
    seen = set()
    durations = []
    pages = set()
    for _ in range(next(tokens)):
        duration = 0
        for _ in range(next(tokens)):
            control, csb, fmt, count, slots, page, first, last = [next(tokens) for _ in range(8)]
            if control != 0 or csb != 1:
                raise ValueError("This audit profile uses control=0 and CSB=1")
            if fmt not in (0, 1) or not 0 <= page <= 3 or not 0 <= first <= last <= 2047:
                raise ValueError("Invalid format, page or within-page AID")
            if not 0 <= count <= (2047 if fmt else 255):
                raise ValueError("Slot duration count overflow")
            if not 1 <= slots <= (7 if fmt else 63):
                raise ValueError("Slot count overflow")
            duration += (500 + 120 * count) * slots
            pages.add(page)
            for local in range(first, last + 1):
                aid = (page << 11) | local
                if aid == 0 or aid in seen:
                    raise ValueError("Reserved or duplicate AID: %d" % aid)
                seen.add(aid)
        if duration >= 102400:
            raise ValueError("RAW duration leaves no room in the beacon interval")
        durations.append(duration)
    if next(tokens, None) is not None or seen != set(range(1, expected + 1)):
        raise ValueError("Trailing fields or incomplete AID coverage")
    return {"stations": len(seen), "pages": sorted(pages),
            "raw_duration_us_per_rps": durations, "field_and_coverage_check": "PASS",
            "tim_scheduling_validated": False, "ieee_conformance_certified": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("file")
    parser.add_argument("--stations", required=True, type=int)
    args = parser.parse_args()
    print(json.dumps(inspect(args.file, args.stations), indent=2))
