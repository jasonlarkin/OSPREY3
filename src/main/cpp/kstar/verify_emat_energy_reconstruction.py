#!/usr/bin/env python3
"""
Verify EnergyMatrix bindings are self-consistent:

Check that:
  compute_energy(conf) ~= const_term
                           + sum_pos one_body(pos, conf[pos])
                           + sum_{pos1>pos2} pairwise(pos1, conf[pos1], pos2, conf[pos2])

This validates:
  - get_one_body()
  - get_pairwise()
  - compute_energy()

Requires: kstar_cpp Python module (pybind11 bindings).
"""

from __future__ import annotations

import argparse
import os
import random
import sys
from typing import List


def _add_build_to_syspath() -> None:
    build_dir = os.path.join(os.path.dirname(__file__), "../../../../build/cpp/kstar-python")
    if os.path.exists(build_dir):
        sys.path.insert(0, build_dir)


def _random_conf(emat, rng: random.Random) -> List[int]:
    npos = emat.get_num_positions()
    conf: List[int] = [0] * npos
    for pos in range(npos):
        nconf = emat.get_num_confs_at_pos(pos)
        if nconf <= 0:
            raise RuntimeError(f"Position {pos} has {nconf} conformations")
        conf[pos] = rng.randrange(nconf)
    return conf


def _reconstruct_energy(emat, conf: List[int]) -> float:
    npos = emat.get_num_positions()
    e = float(emat.get_const_term())
    for pos in range(npos):
        e += float(emat.get_one_body(pos, conf[pos]))
    for pos1 in range(1, npos):
        c1 = conf[pos1]
        for pos2 in range(0, pos1):
            c2 = conf[pos2]
            e += float(emat.get_pairwise(pos1, c1, pos2, c2))
    return e


def main() -> int:
    _add_build_to_syspath()
    try:
        import kstar_cpp  # type: ignore
    except Exception as e:
        print(f"ERROR: failed to import kstar_cpp: {e}")
        return 1

    ap = argparse.ArgumentParser(description="Verify EnergyMatrix energy reconstruction matches compute_energy()")
    ap.add_argument("emat", help="Path to .emat.bin")
    ap.add_argument("--samples", type=int, default=50, help="Number of random confs to test (default: 50)")
    ap.add_argument("--seed", type=int, default=1, help="RNG seed (default: 1)")
    ap.add_argument("--tol", type=float, default=1e-6, help="Absolute tolerance (default: 1e-6)")
    args = ap.parse_args()

    emat = kstar_cpp.load_energy_matrix(args.emat)
    rng = random.Random(int(args.seed))

    max_abs_err = 0.0
    worst = None

    for _ in range(int(args.samples)):
        conf = _random_conf(emat, rng)
        e1 = float(emat.compute_energy(conf))
        e2 = float(_reconstruct_energy(emat, conf))
        err = abs(e1 - e2)
        if err > max_abs_err:
            max_abs_err = err
            worst = (conf, e1, e2)

    print(f"EnergyMatrix: {args.emat}")
    print(f"samples={int(args.samples)} seed={int(args.seed)} tol={float(args.tol):g}")
    print(f"max_abs_err={max_abs_err:.12g}")

    if worst is not None and max_abs_err > float(args.tol):
        conf, e1, e2 = worst
        print("FAIL: reconstruction mismatch")
        print(f"compute_energy={e1:.12g}")
        print(f"reconstructed={e2:.12g}")
        print(f"conf={conf}")
        return 2

    print("PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

