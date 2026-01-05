#!/usr/bin/env python3
"""
Repeatable PartitionFunction suite over .emat.bin files.

For each input energy matrix:
- run A* Baseline and A* Fast at each epsilon
- check invariants (bounds, delta range, converged predicate)
- check Baseline vs Fast parity (results match; timing differs)
- write CSV summary
"""

from __future__ import annotations

import argparse
import csv
import glob
import os
import sys
import time
from dataclasses import dataclass
from typing import Iterable, List, Optional, Sequence, Tuple


def _add_build_to_syspath() -> None:
    build_dir = os.path.join(os.path.dirname(__file__), "../../../../build/cpp/kstar-python")
    if os.path.exists(build_dir):
        sys.path.insert(0, build_dir)


def _parse_floats_csv(s: str) -> List[float]:
    out: List[float] = []
    for part in s.split(","):
        part = part.strip()
        if not part:
            continue
        out.append(float(part))
    return out


def _iter_inputs(paths: List[str], glob_pat: str) -> List[str]:
    out: List[str] = []
    if paths:
        out.extend(paths)
    if glob_pat:
        out.extend(sorted(glob.glob(glob_pat)))
    # De-dupe, preserve order
    seen = set()
    uniq: List[str] = []
    for p in out:
        if p not in seen:
            seen.add(p)
            uniq.append(p)
    return uniq


@dataclass(frozen=True)
class RunRow:
    emat: str
    epsilon: float
    variant: str
    allow_exact_enum: bool
    lower: float
    upper: float
    delta: float
    num_confs: int
    converged: bool
    time_s: float
    invariant_ok: bool
    invariant_msg: str


def _check_invariants(row: RunRow) -> Tuple[bool, str]:
    if row.lower > row.upper:
        return False, "lower>upper"
    if row.delta < -1e-12 or row.delta > 1.0 + 1e-12:
        return False, "delta_out_of_range"
    expected_conv = row.delta <= row.epsilon + 1e-15
    if row.converged != expected_conv:
        return False, "converged_mismatch"
    return True, ""


def main() -> int:
    _add_build_to_syspath()
    try:
        import kstar_cpp  # type: ignore
    except Exception as e:
        print(f"ERROR: failed to import kstar_cpp: {e}")
        return 1

    ap = argparse.ArgumentParser(description="Run PartitionFunction suite over .emat.bin inputs and write CSV")
    ap.add_argument("paths", nargs="*", help="Paths to .emat.bin files (optional if --glob is used)")
    ap.add_argument("--glob", dest="glob_pat", default="*.emat.bin", help='Glob pattern (default: "*.emat.bin")')
    ap.add_argument("--epsilons", default="0.001,0.005,0.01,0.02,0.05", help="Comma-separated epsilons")
    ap.add_argument(
        "--allow-exact-enum",
        action="store_true",
        help="Allow exact enumeration shortcut (default: off; suite prefers A* path)",
    )
    ap.add_argument("--out", default="pfunc_suite.csv", help="Output CSV path (default: pfunc_suite.csv)")
    ap.add_argument("--tol", type=float, default=1e-9, help="Parity tolerance for float fields (default: 1e-9)")
    args = ap.parse_args()

    epsilons = _parse_floats_csv(args.epsilons)
    inputs = _iter_inputs(args.paths, args.glob_pat)
    if not inputs:
        print("ERROR: no inputs found")
        return 2

    pfunc = kstar_cpp.PartitionFunction()

    # Prepare CSV
    out_path = args.out
    fieldnames = [
        "emat",
        "epsilon",
        "variant",
        "allow_exact_enum",
        "lower",
        "upper",
        "delta",
        "num_confs",
        "converged",
        "time_s",
        "invariant_ok",
        "invariant_msg",
        "parity_ok",
        "parity_msg",
        "baseline_time_s",
        "fast_time_s",
        "speedup_fast_over_baseline",
    ]

    rows: List[RunRow] = []

    for emat_path in inputs:
        try:
            emat = kstar_cpp.load_energy_matrix(emat_path)
        except Exception as e:
            # Emit a single failure row per file
            bad = RunRow(
                emat=emat_path,
                epsilon=float("nan"),
                variant="load_failed",
                allow_exact_enum=bool(args.allow_exact_enum),
                lower=float("nan"),
                upper=float("nan"),
                delta=float("nan"),
                num_confs=0,
                converged=False,
                time_s=0.0,
                invariant_ok=False,
                invariant_msg=f"load_failed:{e}",
            )
            rows.append(bad)
            continue

        for eps in epsilons:
            results_by_variant = {}
            times_by_variant = {}

            for variant_name, variant_enum in [
                ("baseline", kstar_cpp.AStarVariant.Baseline),
                ("fast", kstar_cpp.AStarVariant.Fast),
            ]:
                opts = kstar_cpp.PartitionFunctionOptions()
                opts.allow_exact_enumeration = bool(args.allow_exact_enum)
                opts.astar_variant = variant_enum

                t0 = time.perf_counter()
                r = pfunc.compute(
                    emat,
                    epsilon=float(eps),
                    method=kstar_cpp.PartitionFunctionMethod.AStar,
                    options=opts,
                )
                dt = time.perf_counter() - t0

                rr = RunRow(
                    emat=emat_path,
                    epsilon=float(eps),
                    variant=variant_name,
                    allow_exact_enum=bool(args.allow_exact_enum),
                    lower=float(r.lower_bound),
                    upper=float(r.upper_bound),
                    delta=float(r.delta),
                    num_confs=int(r.num_confs),
                    converged=bool(r.converged),
                    time_s=float(dt),
                    invariant_ok=True,
                    invariant_msg="",
                )
                ok, msg = _check_invariants(rr)
                rr = RunRow(**{**rr.__dict__, "invariant_ok": ok, "invariant_msg": msg})

                results_by_variant[variant_name] = rr
                times_by_variant[variant_name] = float(dt)
                rows.append(rr)

            # Parity check at this epsilon
            b = results_by_variant.get("baseline")
            f = results_by_variant.get("fast")
            if b is None or f is None:
                continue

            tol = float(args.tol)
            parity_ok = True
            parity_msg = ""
            if abs(b.lower - f.lower) > tol:
                parity_ok = False
                parity_msg = "lower_mismatch"
            elif abs(b.upper - f.upper) > tol:
                parity_ok = False
                parity_msg = "upper_mismatch"
            elif abs(b.delta - f.delta) > tol:
                parity_ok = False
                parity_msg = "delta_mismatch"
            elif b.num_confs != f.num_confs:
                parity_ok = False
                parity_msg = "num_confs_mismatch"
            elif b.converged != f.converged:
                parity_ok = False
                parity_msg = "converged_mismatch"

            baseline_t = times_by_variant.get("baseline", float("nan"))
            fast_t = times_by_variant.get("fast", float("nan"))
            speedup = (baseline_t / fast_t) if (fast_t and fast_t > 0.0) else float("nan")

            # Write one parity summary row to CSV by attaching parity fields to the baseline row.
            # (Keeps one summary per (emat, epsilon).)
            rows.append(
                RunRow(
                    emat=emat_path,
                    epsilon=float(eps),
                    variant="parity_summary",
                    allow_exact_enum=bool(args.allow_exact_enum),
                    lower=b.lower,
                    upper=b.upper,
                    delta=b.delta,
                    num_confs=b.num_confs,
                    converged=b.converged,
                    time_s=0.0,
                    invariant_ok=parity_ok,
                    invariant_msg=parity_msg,
                )
            )

    # Emit CSV: per-run rows + parity_summary rows.
    with open(out_path, "w", newline="") as f_csv:
        w = csv.DictWriter(f_csv, fieldnames=fieldnames)
        w.writeheader()

        # Build a lookup for baseline/fast times for parity_summary lines.
        # Key: (emat, epsilon) -> (baseline_t, fast_t)
        time_map = {}
        for r in rows:
            if r.variant in ("baseline", "fast"):
                time_map.setdefault((r.emat, r.epsilon), {})[r.variant] = r.time_s

        for r in rows:
            parity_ok = ""
            parity_msg = ""
            baseline_t = ""
            fast_t = ""
            speedup = ""

            if r.variant == "parity_summary":
                # In this row, invariant_ok/msg carry parity status.
                parity_ok = "1" if r.invariant_ok else "0"
                parity_msg = r.invariant_msg
                t = time_map.get((r.emat, r.epsilon), {})
                bt = float(t.get("baseline", float("nan")))
                ft = float(t.get("fast", float("nan")))
                baseline_t = f"{bt:.12g}" if bt == bt else ""
                fast_t = f"{ft:.12g}" if ft == ft else ""
                if ft and ft > 0.0 and bt == bt and ft == ft:
                    speedup = f"{(bt / ft):.6f}"

            w.writerow(
                {
                    "emat": r.emat,
                    "epsilon": f"{r.epsilon:.12g}" if r.epsilon == r.epsilon else "",
                    "variant": r.variant,
                    "allow_exact_enum": "1" if r.allow_exact_enum else "0",
                    "lower": f"{r.lower:.12g}" if r.lower == r.lower else "",
                    "upper": f"{r.upper:.12g}" if r.upper == r.upper else "",
                    "delta": f"{r.delta:.12g}" if r.delta == r.delta else "",
                    "num_confs": str(r.num_confs),
                    "converged": "1" if r.converged else "0",
                    "time_s": f"{r.time_s:.12g}" if r.time_s else "",
                    "invariant_ok": "1" if r.invariant_ok else "0",
                    "invariant_msg": r.invariant_msg,
                    "parity_ok": parity_ok,
                    "parity_msg": parity_msg,
                    "baseline_time_s": baseline_t,
                    "fast_time_s": fast_t,
                    "speedup_fast_over_baseline": speedup,
                }
            )

    print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

