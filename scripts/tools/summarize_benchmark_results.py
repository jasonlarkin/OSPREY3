#!/usr/bin/env python3
"""
Summarize benchmark_results.csv produced by scripts/tools/benchmark_comprehensive.sh.

Focus:
- Verify repeated-run count per (version, system_size)
- Report mean/stddev/CV% so variance plots are interpretable
- Provide scalar-relative speedups from mean timings

Exit codes:
- 0: OK (or non-strict mode with issues)
- 2: strict mode detected missing/extra runs for any group
"""

from __future__ import annotations

import argparse
import csv
import math
import statistics
import sys
from collections import defaultdict
from dataclasses import dataclass
from pathlib import Path


CANONICAL_SIZE_ORDER = {
    "small": 0,
    "medium": 1,
    "large": 2,
    "xlarge": 3,
    "xxlarge": 4,
}


def system_size_sort_key(name: str) -> int:
    return CANONICAL_SIZE_ORDER.get(name, 999)


@dataclass(frozen=True)
class GroupKey:
    version: str
    system_size: str


@dataclass
class GroupStats:
    n: int
    mean: float
    median: float
    stdev: float
    min_v: float
    max_v: float

    @property
    def cv_percent(self) -> float:
        if self.mean <= 0:
            return 0.0
        return (self.stdev / self.mean) * 100.0


def load_times(csv_path: Path) -> dict[GroupKey, list[float]]:
    groups: dict[GroupKey, list[float]] = defaultdict(list)

    with csv_path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        required = {"version", "system_size", "time_us_per_iter"}
        if reader.fieldnames is None or not required.issubset(set(reader.fieldnames)):
            raise ValueError(
                f"{csv_path} missing required columns {sorted(required)}; "
                f"found {reader.fieldnames}"
            )

        for row in reader:
            try:
                t = float(row["time_us_per_iter"])
            except Exception:
                continue
            if not math.isfinite(t) or t <= 0:
                continue
            k = GroupKey(row["version"].strip(), row["system_size"].strip())
            groups[k].append(t)

    return groups


def compute_stats(groups: dict[GroupKey, list[float]]) -> dict[GroupKey, GroupStats]:
    out: dict[GroupKey, GroupStats] = {}
    for k, times in groups.items():
        times_sorted = sorted(times)
        n = len(times_sorted)
        mean = statistics.mean(times_sorted) if n else float("nan")
        median = statistics.median(times_sorted) if n else float("nan")
        stdev = statistics.stdev(times_sorted) if n > 1 else 0.0
        out[k] = GroupStats(
            n=n,
            mean=mean,
            median=median,
            stdev=stdev,
            min_v=times_sorted[0] if n else float("nan"),
            max_v=times_sorted[-1] if n else float("nan"),
        )
    return out


def main(argv: list[str]) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "csv",
        nargs="?",
        default="benchmark_results.csv",
        help="Path to benchmark results CSV (default: benchmark_results.csv)",
    )
    ap.add_argument(
        "--expected-runs",
        type=int,
        default=None,
        help="Expected number of runs per (version, system_size). If set, validates n.",
    )
    ap.add_argument(
        "--strict",
        action="store_true",
        help="Exit non-zero if any group does not match --expected-runs.",
    )
    ap.add_argument(
        "--versions",
        default="scalar,avx2,avx512",
        help="Comma-separated versions to include in speedup table (default: scalar,avx2,avx512)",
    )
    args = ap.parse_args(argv)

    csv_path = Path(args.csv)
    groups = load_times(csv_path)
    stats = compute_stats(groups)

    versions = [v.strip() for v in args.versions.split(",") if v.strip()]
    system_sizes = sorted({k.system_size for k in stats.keys()}, key=system_size_sort_key)

    # Validate run counts
    bad_groups: list[tuple[GroupKey, int]] = []
    if args.expected_runs is not None:
        for k in sorted(stats.keys(), key=lambda kk: (system_size_sort_key(kk.system_size), kk.version)):
            n = stats[k].n
            if n != args.expected_runs:
                bad_groups.append((k, n))

    print("=== Benchmark results summary ===")
    print(f"csv: {csv_path}")
    if args.expected_runs is not None:
        print(f"expected_runs: {args.expected_runs}")
    print("")

    print("Per-group timing stats (time_us_per_iter):")
    print(f"{'version':<8} {'system':<8} {'n':>3} {'mean':>12} {'stdev':>12} {'cv%':>8} {'min':>12} {'max':>12}")
    print("-" * 83)
    for system in system_sizes:
        for version in versions:
            k = GroupKey(version, system)
            if k not in stats:
                continue
            s = stats[k]
            print(
                f"{version:<8} {system:<8} {s.n:>3d} "
                f"{s.mean:>12.3f} {s.stdev:>12.3f} {s.cv_percent:>8.2f} "
                f"{s.min_v:>12.3f} {s.max_v:>12.3f}"
            )
    print("")

    print("Speedup vs scalar (mean-based):")
    print(f"{'system':<8} {'avx2':>10} {'avx512':>10}")
    print("-" * 32)
    for system in system_sizes:
        scalar = stats.get(GroupKey("scalar", system))
        avx2 = stats.get(GroupKey("avx2", system))
        avx512 = stats.get(GroupKey("avx512", system))
        if not scalar or scalar.mean <= 0:
            continue
        sp2 = (scalar.mean / avx2.mean) if (avx2 and avx2.mean > 0) else float("nan")
        sp512 = (scalar.mean / avx512.mean) if (avx512 and avx512.mean > 0) else float("nan")
        print(f"{system:<8} {sp2:>10.3f} {sp512:>10.3f}")
    print("")

    if bad_groups:
        print("Run-count mismatches:")
        for k, n in bad_groups:
            print(f"- {k.version}/{k.system_size}: n={n}")
        if args.strict:
            return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))


