#!/usr/bin/env python3
"""
Plot roofline-style charts from Google Benchmark JSON output for the K* expandInto microbenchmark.

Inputs:
  - JSON produced by:
      ./kstar_expandinto_roofline_bench --benchmark_format=json > out.json

Outputs:
  - roofline plot: FP-add/s vs FP-add/byte (estimated, from benchmark counters)
  - working-set plot: time vs estimated working-set size with cache-capacity reference lines

This script mirrors the style of existing ConfEcalc roofline tooling under scripts/tools/.
"""

from __future__ import annotations

import argparse
import json
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Tuple

import matplotlib.pyplot as plt
import numpy as np


@dataclass(frozen=True)
class Point:
    label: str
    ai: float  # fp_add/byte
    gops: float  # fp_add/s, in G (1e9)
    time_ns: float
    working_set_bytes: float
    num_pos: int
    rc: int
    k_level: int


def parse_benchmark_json(path: Path, label: str) -> List[Point]:
    obj = json.loads(path.read_text())
    benches = obj.get("benchmarks", [])
    out: List[Point] = []

    for b in benches:
        name = str(b.get("name", ""))
        # Accept both synthetic and real-.emat modes.
        if ("BM_AStarFast_ExpandInto_Roofline/") not in name and ("BM_AStarFast_ExpandInto_Roofline_RealEmat/") not in name:
            continue
        run_type = str(b.get("run_type", ""))
        if run_type == "iteration":
            pass
        elif run_type == "aggregate":
            # When benchmarks are run with --benchmark_report_aggregates_only=true, only
            # aggregate rows are present. Use the mean row as the representative point.
            if str(b.get("aggregate_name", "")) != "mean":
                continue
        else:
            continue

        time_ns = float(b.get("real_time", 0.0))
        time_unit = b.get("time_unit", "ns")
        if time_unit == "ns":
            pass
        elif time_unit == "us":
            time_ns *= 1e3
        elif time_unit == "ms":
            time_ns *= 1e6
        elif time_unit == "s":
            time_ns *= 1e9

        counters: Dict[str, float] = {}
        for k, v in b.items():
            if k in {
                "num_pos",
                "rc",
                "k_level",
                "fp_adds_est",
                "bytes_est",
                "ai_est",
                "working_set_bytes_est",
            }:
                try:
                    counters[k] = float(v)
                except Exception:
                    pass

        fp_adds = counters.get("fp_adds_est", 0.0)
        bytes_est = counters.get("bytes_est", 0.0)
        ai = counters.get("ai_est", (fp_adds / bytes_est) if bytes_est > 0 else 0.0)
        ws = counters.get("working_set_bytes_est", 0.0)

        # Ignore aggregate stddev/cv rows and any malformed rows that carry zeroed counters.
        if counters.get("num_pos", 0.0) <= 0.0 or counters.get("rc", 0.0) <= 0.0 or fp_adds <= 0.0 or bytes_est <= 0.0:
            continue

        gops = 0.0
        if time_ns > 0.0:
            gops = (fp_adds / (time_ns * 1e-9)) / 1e9

        out.append(
            Point(
                label=label,
                ai=max(ai, 1e-12),
                gops=max(gops, 1e-12),
                time_ns=time_ns,
                working_set_bytes=max(ws, 1.0),
                num_pos=int(counters.get("num_pos", 0.0)),
                rc=int(counters.get("rc", 0.0)),
                k_level=int(counters.get("k_level", 0.0)),
            )
        )

    return out


def plot_roofline(points: List[Point], out_dir: Path, peak_gops: float, bw_gbs: float) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(figsize=(12, 8), constrained_layout=True)

    ai_range = np.logspace(-4, 2, 2000)
    memory_bound = ai_range * bw_gbs  # (FLOPs/byte) * (GB/s) => GFLOP/s
    compute_bound = np.full_like(ai_range, peak_gops)
    roofline = np.minimum(memory_bound, compute_bound)

    ax.loglog(ai_range, roofline, "k-", linewidth=2, label="Roofline (model)")
    knee = peak_gops / bw_gbs if bw_gbs > 0 else 0.0
    if knee > 0:
        ax.axvline(x=knee, color="r", linestyle="--", alpha=0.6, label=f"knee ({knee:.3g} adds/B)")
    ax.axhline(y=peak_gops, color="g", linestyle="--", alpha=0.6, label=f"peak ({peak_gops:.1f} Gadds/s)")

    by_label: Dict[str, List[Point]] = {}
    for p in points:
        by_label.setdefault(p.label, []).append(p)

    colors = ["#e74c3c", "#2ecc71", "#3498db", "#9b59b6", "#f39c12"]
    for i, (lbl, pts) in enumerate(sorted(by_label.items(), key=lambda kv: kv[0])):
        xs = [p.ai for p in pts]
        ys = [p.gops for p in pts]
        ax.scatter(xs, ys, s=120, alpha=0.75, edgecolors="black", linewidth=1.0, color=colors[i % len(colors)], label=lbl)

    ax.set_xlabel("Arithmetic intensity (estimated FP adds / byte)")
    ax.set_ylabel("Performance (estimated Gadds/s)")
    ax.set_title("K* expandInto roofline (estimated)")
    ax.grid(alpha=0.3, linestyle="--", which="both")
    ax.legend(loc="best")

    out_path = out_dir / "kstar_expandinto_roofline.png"
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def plot_working_set(points: List[Point], out_dir: Path, l1_bytes: int, l2_bytes: int, l3_bytes: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    fig, ax = plt.subplots(figsize=(12, 8), constrained_layout=True)

    by_label: Dict[str, List[Point]] = {}
    for p in points:
        by_label.setdefault(p.label, []).append(p)

    colors = ["#e74c3c", "#2ecc71", "#3498db", "#9b59b6", "#f39c12"]
    for i, (lbl, pts) in enumerate(sorted(by_label.items(), key=lambda kv: kv[0])):
        pts2 = sorted(pts, key=lambda p: p.working_set_bytes)
        xs = [p.working_set_bytes for p in pts2]
        ys = [p.time_ns for p in pts2]
        ax.plot(xs, ys, marker="o", linewidth=1.8, alpha=0.85, color=colors[i % len(colors)], label=lbl)

    ax.set_xscale("log")
    ax.set_yscale("log")

    # Cache-capacity reference lines. Place labels in axes coords to avoid log-scale issues (y=0 is invalid on log).
    for (val, name, col) in [(l1_bytes, "L1D", "#7f8c8d"), (l2_bytes, "L2", "#95a5a6"), (l3_bytes, "L3", "#bdc3c7")]:
        ax.axvline(x=val, linestyle="--", alpha=0.8, color=col)
        ax.text(
            val,
            0.02,
            f" {name}",
            transform=ax.get_xaxis_transform(),  # x in data coords, y in axes coords
            rotation=90,
            va="bottom",
            ha="left",
            color=col,
        )

    # DRAM is not a single “capacity line”; it is the regime where the active working set exceeds LLC.
    # Shade the region to the right of L3 for visual emphasis.
    try:
        x_max = max(p.working_set_bytes for p in points) * 1.1
    except Exception:
        x_max = None
    if x_max is not None and x_max > l3_bytes:
        ax.axvspan(l3_bytes, x_max, color="#dfe6e9", alpha=0.35)
        ax.text(
            (l3_bytes * x_max) ** 0.5,
            0.93,
            "DRAM regime\n(working set > L3)",
            transform=ax.get_xaxis_transform(),
            ha="center",
            va="top",
            fontsize=10,
            color="#2d3436",
        )
    ax.set_xlabel("Estimated working set (bytes)")
    ax.set_ylabel("Time per expandInto (ns)")
    ax.set_title("K* expandInto time vs working-set estimate")
    ax.grid(alpha=0.3, linestyle="--", which="both")
    ax.legend(loc="best")

    out_path = out_dir / "kstar_expandinto_working_set.png"
    fig.savefig(out_path, dpi=150)
    plt.close(fig)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--input",
        action="append",
        required=True,
        help="Input in form label=path.json (repeatable). Example: simd_on=out_on.json",
    )
    ap.add_argument("--out-dir", default="plots", help="Output directory for plots")
    ap.add_argument("--peak-gops", type=float, default=100.0, help="Peak compute ceiling in Gadds/s (model line)")
    ap.add_argument("--bw-gbs", type=float, default=50.0, help="Memory bandwidth ceiling in GB/s (model line)")
    ap.add_argument("--l1-bytes", type=int, default=48 * 1024, help="L1 data cache size (bytes) for reference line")
    ap.add_argument("--l2-bytes", type=int, default=1280 * 1024, help="L2 cache size (bytes) for reference line")
    ap.add_argument("--l3-bytes", type=int, default=6144 * 1024, help="L3 cache size (bytes) for reference line")
    args = ap.parse_args()

    points: List[Point] = []
    for spec in args.input:
        if "=" not in spec:
            raise SystemExit(f"invalid --input: {spec} (expected label=path)")
        label, path_s = spec.split("=", 1)
        p = Path(path_s)
        points.extend(parse_benchmark_json(p, label=label))

    out_dir = Path(args.out_dir)
    plot_roofline(points, out_dir, peak_gops=args.peak_gops, bw_gbs=args.bw_gbs)
    plot_working_set(points, out_dir, l1_bytes=args.l1_bytes, l2_bytes=args.l2_bytes, l3_bytes=args.l3_bytes)


if __name__ == "__main__":
    main()

