#!/usr/bin/env python3
"""
Low-level demo for kstar_cpp bindings:
- load an EnergyMatrix from .emat.bin
- inspect a few one-body / pairwise energies
- compute a PartitionFunction with options (A* baseline/fast, exact-enum gate)
"""

from __future__ import annotations

import argparse
import os
import sys
import time
from typing import List


def _add_build_to_syspath() -> None:
    # Add build directory to sys.path (repo-local, same pattern as other scripts here).
    build_dir = os.path.join(os.path.dirname(__file__), "../../../../build/cpp/kstar-python")
    if os.path.exists(build_dir):
        sys.path.insert(0, build_dir)


def _first_valid_conf_vector(emat) -> List[int]:
    # Choose a simple valid conformation vector: all zeros (if every position has >= 1 conf).
    npos = emat.get_num_positions()
    conf = []
    for pos in range(npos):
        nconf = emat.get_num_confs_at_pos(pos)
        if nconf <= 0:
            raise RuntimeError(f"Position {pos} has {nconf} conformations")
        conf.append(0)
    return conf


def main() -> int:
    _add_build_to_syspath()

    try:
        import kstar_cpp  # type: ignore
    except Exception as e:
        print(f"ERROR: failed to import kstar_cpp: {e}")
        print("Fix: ensure build/cpp/kstar-python is on PYTHONPATH, or rebuild the module for your Python version.")
        return 1

    ap = argparse.ArgumentParser(description="Low-level demo for kstar_cpp PartitionFunction")
    ap.add_argument(
        "emat",
        nargs="?",
        default="1GUA11.TestSimplePartitionFunction.complex.emat.bin",
        help="Path to .emat.bin (default: 1GUA11.TestSimplePartitionFunction.complex.emat.bin)",
    )
    ap.add_argument("--epsilon", type=float, default=0.01, help="Convergence threshold on delta (default: 0.01)")
    ap.add_argument(
        "--astar-variant",
        choices=["baseline", "fast"],
        default="fast",
        help="A* implementation variant (default: fast)",
    )
    ap.add_argument(
        "--allow-exact-enum",
        action="store_true",
        help="Allow exact enumeration shortcut on tiny spaces (default: off for demos)",
    )
    ap.add_argument("--print-samples", action="store_true", help="Print sample one-body/pairwise energies")

    args = ap.parse_args()

    emat = kstar_cpp.load_energy_matrix(args.emat)

    npos = emat.get_num_positions()
    print(f"EnergyMatrix: positions={npos}, const_term={emat.get_const_term():.6f}")
    for pos in range(npos):
        print(f"  pos {pos}: num_confs={emat.get_num_confs_at_pos(pos)}")

    # Inspect a single total energy for a valid conformation vector.
    conf = _first_valid_conf_vector(emat)
    e_total = emat.compute_energy(conf)
    print(f"compute_energy(conf=[0]*{npos}) = {e_total:.6f}")

    if args.print_samples:
        # One-body samples: first 3 confs at first 2 positions.
        max_pos = min(npos, 2)
        for pos in range(max_pos):
            nconf = emat.get_num_confs_at_pos(pos)
            for conf_i in range(min(nconf, 3)):
                e1 = emat.get_one_body(pos, conf_i)
                print(f"one_body(pos={pos}, conf={conf_i}) = {e1:.6f}")

        # Pairwise sample if at least 2 positions exist.
        if npos >= 2:
            pos1, pos2 = 1, 0
            n1 = emat.get_num_confs_at_pos(pos1)
            n2 = emat.get_num_confs_at_pos(pos2)
            e2 = emat.get_pairwise(pos1, 0, pos2, 0) if n1 > 0 and n2 > 0 else None
            if e2 is not None:
                print(f"pairwise(pos1={pos1}, conf1=0, pos2={pos2}, conf2=0) = {e2:.6f}")

    opts = kstar_cpp.PartitionFunctionOptions()
    opts.allow_exact_enumeration = bool(args.allow_exact_enum)
    opts.astar_variant = kstar_cpp.AStarVariant.Fast if args.astar_variant == "fast" else kstar_cpp.AStarVariant.Baseline

    pfunc = kstar_cpp.PartitionFunction()
    t0 = time.perf_counter()
    r = pfunc.compute(
        emat,
        epsilon=float(args.epsilon),
        method=kstar_cpp.PartitionFunctionMethod.AStar,
        options=opts,
    )
    elapsed_s = time.perf_counter() - t0

    print(
        "PartitionFunctionResult:",
        f"lower={r.lower_bound:.6f}",
        f"upper={r.upper_bound:.6f}",
        f"delta={r.delta:.6f}",
        f"num_confs={r.num_confs}",
        f"converged={r.converged}",
    )
    print(f"compute_time_s: {elapsed_s:.6f}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

