#!/usr/bin/env python3
"""
Parse outputs from scripts/tools/repro_simd_wsl.sh and generate:
- summary CSV (means/std/min/max/CV for us/iter; derived perf metrics)
- plots (if matplotlib is available)

Usage:
  python3 scripts/tools/plot_repro_simd_results.py perf_results/repro_YYYYMMDD_HHMMSS
  python3 scripts/tools/plot_repro_simd_results.py --latest
"""

from __future__ import annotations

import argparse
import csv
import math
import os
import re
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Optional, Tuple


RE_US_PER_ITER = re.compile(r"us/iter")
RE_FLOAT = re.compile(r"([-+]?\d+(?:\.\d+)?)")


def mean(xs: List[float]) -> float:
    return sum(xs) / len(xs)


def stdev(xs: List[float]) -> float:
    if len(xs) < 2:
        return 0.0
    m = mean(xs)
    var = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return math.sqrt(var)


@dataclass
class SeriesStats:
    n: int
    mean: float
    stdev: float
    min: float
    max: float
    cv: float


def stats(xs: List[float]) -> SeriesStats:
    xs2 = list(xs)
    xs2.sort()
    m = mean(xs2)
    s = stdev(xs2)
    return SeriesStats(
        n=len(xs2),
        mean=m,
        stdev=s,
        min=xs2[0],
        max=xs2[-1],
        cv=(s / m) if m != 0.0 else 0.0,
    )


def parse_us_per_iter_file(path: Path) -> List[float]:
    vals: List[float] = []
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        if "us/iter" not in line:
            continue
        # Lines look like: "[rep 1] 31.0164 us/iter"
        # Don't take the rep index; take the last float on the line.
        nums = RE_FLOAT.findall(line)
        if not nums:
            continue
        vals.append(float(nums[-1]))
    return vals


def parse_perf_stat_csv(path: Path) -> Dict[str, float]:
    """
    perf stat -x, output format is not stable across versions, but we rely on:
    col0: value
    col2: event (e.g. cycles:u)
    """
    out: Dict[str, float] = {}
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        parts = [p.strip() for p in line.split(",")]
        if len(parts) < 3:
            continue
        value_s = parts[0]
        event = parts[2]
        if not event:
            continue
        # skip non-numeric values (e.g. <not supported>)
        try:
            value = float(value_s)
        except ValueError:
            continue
        out[event] = value
    return out


def derive_perf_metrics(events: Dict[str, float]) -> Dict[str, float]:
    # Normalize keys without :u suffix for convenience
    def get(ev: str) -> Optional[float]:
        for k in (ev, f"{ev}:u"):
            if k in events:
                return events[k]
        return None

    cycles = get("cycles")
    inst = get("instructions")
    cache_refs = get("cache-references")
    cache_miss = get("cache-misses")
    br = get("branch-instructions")
    br_miss = get("branch-misses")

    metrics: Dict[str, float] = {}
    if cycles and inst and cycles != 0.0:
        metrics["ipc"] = inst / cycles
    if cache_refs and cache_miss and cache_refs != 0.0:
        metrics["cache_miss_rate"] = cache_miss / cache_refs
    if br and br_miss and br != 0.0:
        metrics["branch_miss_rate"] = br_miss / br
    return metrics


def find_latest_repro_dir(repo_root: Path) -> Path:
    base = repo_root / "perf_results"
    candidates = sorted(base.glob("repro_*"), key=lambda p: p.name)
    if not candidates:
        raise FileNotFoundError(f"no repro dirs found under: {base}")
    return candidates[-1]


def try_plot(outdir: Path, bench_series: Dict[str, List[float]], perf_ipc: Dict[str, float]) -> None:
    try:
        import matplotlib.pyplot as plt  # type: ignore
    except Exception:
        (outdir / "PLOTS_SKIPPED.txt").write_text(
            "matplotlib not available; wrote CSV summaries only.\n", encoding="utf-8"
        )
        return

    # Boxplot for us/iter
    labels = list(bench_series.keys())
    data = [bench_series[k] for k in labels]
    plt.figure(figsize=(9, 4))
    plt.boxplot(data, labels=labels, showfliers=True)
    plt.ylabel("us/iter")
    plt.title("ConfEcalc benchmark_*_only reproducibility")
    plt.tight_layout()
    plt.savefig(outdir / "bench_us_per_iter_boxplot.png", dpi=160)
    plt.close()

    # IPC bar plot if present
    if perf_ipc:
        plt.figure(figsize=(7, 3.5))
        xs = list(perf_ipc.keys())
        ys = [perf_ipc[k] for k in xs]
        plt.bar(xs, ys)
        plt.ylabel("IPC (instructions/cycle)")
        plt.title("perf stat derived IPC (user-space)")
        plt.tight_layout()
        plt.savefig(outdir / "perf_ipc_bar.png", dpi=160)
        plt.close()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("repro_dir", nargs="?", help="Path to perf_results/repro_* dir")
    ap.add_argument("--latest", action="store_true", help="Use latest perf_results/repro_* dir")
    args = ap.parse_args()

    repo_root = Path(__file__).resolve().parents[2]
    if args.latest:
        repro_dir = find_latest_repro_dir(repo_root)
    elif args.repro_dir:
        repro_dir = Path(args.repro_dir).resolve()
    else:
        raise SystemExit("Provide repro_dir or --latest")

    outdir = repro_dir / "plots"
    outdir.mkdir(parents=True, exist_ok=True)

    bench_files = {
        "scalar_only": repro_dir / "benchmark_scalar_only.txt",
        "avx2_only": repro_dir / "benchmark_avx2_only.txt",
        "avx512_only": repro_dir / "benchmark_avx512_only.txt",
    }

    bench_series: Dict[str, List[float]] = {}
    bench_stats: Dict[str, SeriesStats] = {}
    for k, p in bench_files.items():
        if not p.exists():
            continue
        vals = parse_us_per_iter_file(p)
        if vals:
            bench_series[k] = vals
            bench_stats[k] = stats(vals)

    # perf stat CSVs (from repro_simd_wsl.sh)
    perf_files = {
        "scalar_only": repro_dir / "perf_scalar_only.csv",
        "avx2_only": repro_dir / "perf_avx2_only.csv",
        "avx512_only": repro_dir / "perf_avx512_only.csv",
    }

    perf_events: Dict[str, Dict[str, float]] = {}
    perf_derived: Dict[str, Dict[str, float]] = {}
    for k, p in perf_files.items():
        if not p.exists():
            continue
        evs = parse_perf_stat_csv(p)
        if evs:
            perf_events[k] = evs
            perf_derived[k] = derive_perf_metrics(evs)

    # Write CSV summary
    summary_csv = repro_dir / "repro_summary.csv"
    with summary_csv.open("w", newline="", encoding="utf-8") as f:
        w = csv.writer(f)
        w.writerow(["group", "version", "metric", "value"])

        for ver, st in bench_stats.items():
            w.writerow(["bench", ver, "n", st.n])
            w.writerow(["bench", ver, "mean_us_per_iter", f"{st.mean:.6f}"])
            w.writerow(["bench", ver, "stdev_us_per_iter", f"{st.stdev:.6f}"])
            w.writerow(["bench", ver, "cv", f"{st.cv:.6f}"])
            w.writerow(["bench", ver, "min_us_per_iter", f"{st.min:.6f}"])
            w.writerow(["bench", ver, "max_us_per_iter", f"{st.max:.6f}"])

        for ver, d in perf_derived.items():
            for metric, value in d.items():
                w.writerow(["perf", ver, metric, f"{value:.6f}"])

    # Extract IPC into a single dict for plotting convenience
    perf_ipc = {ver: d["ipc"] for ver, d in perf_derived.items() if "ipc" in d}
    try_plot(outdir, bench_series, perf_ipc)

    print(f"Wrote: {summary_csv}")
    print(f"Plots dir: {outdir}")


if __name__ == "__main__":
    main()

