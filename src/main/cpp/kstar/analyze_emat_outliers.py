#!/usr/bin/env python3
"""
Repeatable `.emat.bin` analysis:
- summarize one-body energy distribution
- print top outliers with (pos, conf)

Requires: kstar_cpp Python module (pybind11 bindings).
"""

from __future__ import annotations

import argparse
import math
import os
import sys
from typing import Iterable, List, Sequence, Tuple


def _add_build_to_syspath() -> None:
    build_dir = os.path.join(os.path.dirname(__file__), "../../../../build/cpp/kstar-python")
    if os.path.exists(build_dir):
        sys.path.insert(0, build_dir)


def _parse_percentiles(s: str) -> List[float]:
    out: List[float] = []
    for part in s.split(","):
        part = part.strip()
        if not part:
            continue
        out.append(float(part))
    if not out:
        out = [50.0, 90.0, 95.0, 99.0]
    return out


def _percentile(sorted_vals: Sequence[float], p: float) -> float:
    if not sorted_vals:
        return float("nan")
    if p <= 0.0:
        return float(sorted_vals[0])
    if p >= 100.0:
        return float(sorted_vals[-1])
    i = int(round((p / 100.0) * (len(sorted_vals) - 1)))
    i = max(0, min(i, len(sorted_vals) - 1))
    return float(sorted_vals[i])


def main() -> int:
    _add_build_to_syspath()
    try:
        import kstar_cpp  # type: ignore
    except Exception as e:
        print(f"ERROR: failed to import kstar_cpp: {e}")
        return 1

    ap = argparse.ArgumentParser(description="Analyze one-body outliers in an EnergyMatrix (.emat.bin)")
    ap.add_argument("emat", help="Path to .emat.bin")
    ap.add_argument("--top", type=int, default=20, help="Number of top energies to print (default: 20)")
    ap.add_argument(
        "--percentiles",
        type=str,
        default="50,90,95,99",
        help='Percentiles to print, comma-separated (default: "50,90,95,99")',
    )
    ap.add_argument(
        "--max-positions",
        type=int,
        default=0,
        help="Limit positions scanned (0 = no limit, default: 0)",
    )
    ap.add_argument(
        "--max-confs",
        type=int,
        default=0,
        help="Limit conformations per position scanned (0 = no limit, default: 0)",
    )
    args = ap.parse_args()

    emat = kstar_cpp.load_energy_matrix(args.emat)
    npos = emat.get_num_positions()
    if args.max_positions > 0:
        npos = min(npos, args.max_positions)

    entries: List[Tuple[float, int, int]] = []
    finite: List[float] = []

    for pos in range(npos):
        nconf = emat.get_num_confs_at_pos(pos)
        if args.max_confs > 0:
            nconf = min(nconf, args.max_confs)
        for conf in range(nconf):
            e = float(emat.get_one_body(pos, conf))
            entries.append((e, pos, conf))
            if math.isfinite(e):
                finite.append(e)

    entries.sort(key=lambda t: t[0], reverse=True)
    finite.sort()

    print(f"EnergyMatrix: {args.emat}")
    print(f"positions_scanned={npos} const_term={emat.get_const_term():.6f}")
    print(f"one_body_values_scanned={len(entries)} finite={len(finite)}")

    if not finite:
        print("No finite one-body energies found.")
        return 0

    percentiles = _parse_percentiles(args.percentiles)
    pct_parts = []
    for p in percentiles:
        pct_parts.append(f"p{p:g}={_percentile(finite, p):.6f}")
    print(" ".join(pct_parts))
    print(f"min={finite[0]:.6f} max={finite[-1]:.6f}")

    topn = max(0, int(args.top))
    if topn > 0:
        print(f"\nTop {topn} one-body energies:")
        for e, pos, conf in entries[:topn]:
            print(f"e={e:.6f} pos={pos} conf={conf}")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

