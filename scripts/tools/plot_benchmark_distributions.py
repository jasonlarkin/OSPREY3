#!/usr/bin/env python3
"""
Plot repeated-run timing distributions from benchmark_results.csv.

Output:
- plots/<label>/timing_distributions.png   (label optional via OSPREY_EXPERIMENT_LABEL)
"""

from __future__ import annotations

import csv
import os
import random
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np


CANONICAL_SIZE_ORDER = {
    "small": 0,
    "medium": 1,
    "large": 2,
    "xlarge": 3,
    "xxlarge": 4,
}


def system_size_sort_key(name: str) -> int:
    return CANONICAL_SIZE_ORDER.get(name, 999)


def get_experiment_label() -> str:
    return (os.environ.get("OSPREY_EXPERIMENT_LABEL") or "").strip()


def get_plots_dir(base_plots_dir: Path) -> Path:
    label = get_experiment_label()
    return (base_plots_dir / label) if label else base_plots_dir


def load_runs(csv_file: Path) -> dict[tuple[str, str], list[float]]:
    runs: dict[tuple[str, str], list[float]] = defaultdict(list)  # (version, system_size) -> times
    with csv_file.open("r", newline="") as f:
        reader = csv.DictReader(f)
        for row in reader:
            v = (row.get("version") or "").strip()
            s = (row.get("system_size") or "").strip()
            if not v or not s:
                continue
            try:
                t = float(row.get("time_us_per_iter") or 0.0)
            except Exception:
                continue
            if t <= 0:
                continue
            runs[(v, s)].append(t)
    return runs


def plot_distributions(runs: dict[tuple[str, str], list[float]], output_dir: Path) -> Path | None:
    versions = ["scalar", "avx2", "avx512"]
    colors = {"scalar": "#e74c3c", "avx2": "#2ecc71", "avx512": "#3498db"}

    systems = sorted({s for (_, s) in runs.keys()}, key=system_size_sort_key)
    if not systems:
        return None

    # Prepare data per system, per version
    data_by_system: dict[str, list[list[float]]] = {}
    for s in systems:
        data_by_system[s] = [runs.get((v, s), []) for v in versions]

    # Layout: grouped boxplots per system_size
    group_gap = 1.4
    within_gap = 0.28
    box_width = 0.22

    fig, ax = plt.subplots(figsize=(12, 6.5))

    positions: list[float] = []
    box_data: list[list[float]] = []
    box_colors: list[str] = []

    # deterministic jitter so reruns are stable
    rng = random.Random(0)

    for si, s in enumerate(systems):
        base = si * group_gap
        for vi, v in enumerate(versions):
            vals = data_by_system[s][vi]
            if not vals:
                continue
            pos = base + (vi - 1) * within_gap
            positions.append(pos)
            box_data.append(vals)
            box_colors.append(colors[v])

            # raw points with jitter (shows outliers explicitly)
            for t in vals:
                jitter = (rng.random() - 0.5) * (box_width * 0.9)
                ax.plot(pos + jitter, t, marker="o", markersize=3.5, alpha=0.55, color="black", linewidth=0)

    if not box_data:
        return None

    bp = ax.boxplot(
        box_data,
        positions=positions,
        widths=box_width,
        patch_artist=True,
        showfliers=False,  # raw points already show fliers explicitly
        medianprops={"color": "black", "linewidth": 1.8},
        whiskerprops={"linewidth": 1.2},
        capprops={"linewidth": 1.2},
    )

    for patch, c in zip(bp["boxes"], box_colors):
        patch.set_facecolor(c)
        patch.set_alpha(0.75)
        patch.set_edgecolor("black")
        patch.set_linewidth(1.0)

    # X ticks at group centers
    xticks = [si * group_gap for si in range(len(systems))]
    ax.set_xticks(xticks)
    ax.set_xticklabels(systems, fontsize=10)

    ax.set_xlabel("System size preset", fontsize=12, fontweight="bold")
    ax.set_ylabel("Time per iteration (μs)", fontsize=12, fontweight="bold")
    ax.set_title("Timing distributions across repeated runs (boxplot + raw points)", fontsize=13, fontweight="bold")
    ax.grid(axis="y", alpha=0.25, linestyle="--")
    ax.set_yscale("log")

    # Legend
    handles = [
        plt.Line2D([0], [0], marker="s", color="w", label=v.upper(), markerfacecolor=colors[v], markeredgecolor="black", markersize=10)
        for v in versions
    ]
    ax.legend(handles=handles, fontsize=10, loc="upper left")

    plt.tight_layout()
    out = output_dir / "timing_distributions.png"
    plt.savefig(out, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"Saved: {out}")
    return out


def main(argv: list[str]) -> int:
    csv_path = Path(argv[1]) if len(argv) > 1 else (Path(__file__).parent.parent.parent / "benchmark_results.csv")
    if not csv_path.exists():
        print(f"Error: CSV not found: {csv_path}", file=sys.stderr)
        return 1

    output_dir = get_plots_dir(csv_path.parent / "plots")
    output_dir.mkdir(parents=True, exist_ok=True)

    runs = load_runs(csv_path)
    out = plot_distributions(runs, output_dir)
    if out is None:
        print("No valid timing rows found; no plot generated.", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv))


