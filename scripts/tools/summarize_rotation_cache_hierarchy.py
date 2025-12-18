#!/usr/bin/env python3
"""
Summarize scalar vs SIMD results from:
  tmp/rotation_cache_hierarchy.csv

Prints per-(op,size) comparisons:
  - cycle speedup (scalar cycles / simd cycles)
  - task-clock speedup
  - IPC
  - L1 miss rate
  - cache miss rate

Notes:
  - LLC events may be unsupported; those fields are stored as -1.
"""

import csv
from collections import defaultdict
from pathlib import Path


def f(x: str) -> float:
    try:
        return float(x)
    except Exception:
        return 0.0


def i(x: str) -> int:
    try:
        return int(float(x))
    except Exception:
        return 0


def main() -> None:
    repo = Path(__file__).resolve().parents[2]
    csv_path = repo / "tmp" / "rotation_cache_hierarchy.csv"
    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    rows = list(csv.DictReader(csv_path.open("r", newline="")))
    by = defaultdict(dict)
    for r in rows:
        key = (r["op"], int(r["num_vectors"]))
        by[key][r["impl"]] = r

    print("op,size,cycle_speedup,task_clock_speedup,ipc_scalar,ipc_simd,l1_miss_scalar,l1_miss_simd,cache_miss_scalar,cache_miss_simd")
    for (op, size) in sorted(by.keys(), key=lambda x: (x[0], x[1])):
        d = by[(op, size)]
        if "scalar" not in d or "simd" not in d:
            continue
        s = d["scalar"]
        v = d["simd"]

        cyc_s = i(s["cycles"])
        cyc_v = i(v["cycles"])
        tc_s = f(s["task_clock_ms"])
        tc_v = f(v["task_clock_ms"])

        cyc_sp = (cyc_s / cyc_v) if cyc_v else 0.0
        tc_sp = (tc_s / tc_v) if tc_v else 0.0

        print(
            f"{op},{size},"
            f"{cyc_sp:.3f},{tc_sp:.3f},"
            f"{f(s['ipc']):.3f},{f(v['ipc']):.3f},"
            f"{f(s['L1_miss_rate']):.3f},{f(v['L1_miss_rate']):.3f},"
            f"{f(s['cache_miss_rate']):.2f},{f(v['cache_miss_rate']):.2f}"
        )


if __name__ == "__main__":
    main()


