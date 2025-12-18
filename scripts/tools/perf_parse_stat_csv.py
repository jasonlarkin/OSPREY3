#!/usr/bin/env python3
"""
Parse `perf stat -x,` CSV output and extract counters by event name.

This is a small glue tool used by shell scripts in scripts/tools/.

Notes:
- perf may emit events without the requested ":u" suffix; we normalize it away.
- Unsupported / not-counted counters are treated as missing.
"""

from __future__ import annotations

import argparse
from pathlib import Path
from typing import Dict, List, Optional


MISSING_TOKENS = {"", "<not supported>", "not supported", "<not counted>"}


def norm_event(ev: str) -> str:
    ev = ev.strip()
    if ev.endswith(":u"):
        ev = ev[:-2]
    return ev


def parse_num(s: str) -> Optional[float]:
    s = s.strip()
    if s in MISSING_TOKENS:
        return None
    s = s.replace(",", "")
    try:
        return float(s)
    except Exception:
        return None


def parse_perf_csv_lines(lines: List[str]) -> Dict[str, Optional[float]]:
    vals: Dict[str, Optional[float]] = {}
    for line in lines:
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
            continue
        v, _unit, ev = parts[0], parts[1], parts[2]
        if not ev or ev.startswith("#"):
            continue
        key = norm_event(ev)
        num = parse_num(v)
        if num is None:
            vals.setdefault(key, None)
            continue
        vals.setdefault(key, num)
    return vals


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True, help="Path to file captured from `perf stat -x, ...`")
    ap.add_argument(
        "--missing",
        choices=["zero", "minus1"],
        default="zero",
        help="How to encode missing counters in output",
    )
    ap.add_argument(
        "events",
        nargs="+",
        help="Event names to extract (e.g., cycles, instructions, LLC-load-misses). ':u' suffix is optional.",
    )
    args = ap.parse_args()

    p = Path(args.input)
    lines = p.read_text(errors="ignore").splitlines()
    vals = parse_perf_csv_lines(lines)

    missing_value = 0 if args.missing == "zero" else -1
    out: List[str] = []
    for ev in args.events:
        key = norm_event(ev)
        v = vals.get(key, None)
        if v is None:
            out.append(str(missing_value))
        else:
            out.append(str(int(v)))
    print(" ".join(out))


if __name__ == "__main__":
    main()


