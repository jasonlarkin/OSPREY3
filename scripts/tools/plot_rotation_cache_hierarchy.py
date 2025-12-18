#!/usr/bin/env python3
"""
Plot cache hierarchy metrics for rotation/normalize benchmarks.

Input:
  ./tmp/rotation_cache_hierarchy.csv (from profile_rotation_cache_hierarchy.sh)

Output:
  ./plots/rotation_cache_hierarchy_<op>.png
"""

import csv
from pathlib import Path
import matplotlib.pyplot as plt

def get_experiment_label():
    import os
    return (os.environ.get("OSPREY_EXPERIMENT_LABEL") or "").strip()

def get_plots_dir(base_plots_dir: Path) -> Path:
    label = get_experiment_label()
    return (base_plots_dir / label) if label else base_plots_dir


def load_rows(path: Path):
    with path.open("r", newline="") as f:
        reader = csv.DictReader(f)
        return list(reader)


def to_float(x: str) -> float:
    try:
        return float(x)
    except Exception:
        return 0.0


def to_int(x: str) -> int:
    try:
        return int(float(x))
    except Exception:
        return 0


def series(rows, x_key, y_key, *, drop_negative=False):
    xs = []
    ys = []
    for r in rows:
        x = to_int(r[x_key])
        y = to_float(r[y_key])
        if drop_negative and y < 0:
            continue
        xs.append(x)
        ys.append(y)
    return xs, ys


def plot_op(rows, op: str, out_dir: Path):
    data = [r for r in rows if r["op"] == op]
    if not data:
        return

    impls = sorted({r["impl"] for r in data})
    colors = {"scalar": "#e74c3c", "simd": "#3498db"}

    # group by impl
    by_impl = {impl: sorted([r for r in data if r["impl"] == impl], key=lambda rr: to_int(rr["num_vectors"])) for impl in impls}

    label = get_experiment_label()
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    title = f"Cache Hierarchy: {op}"
    if label:
        title += f" [{label}]"
    fig.suptitle(title, fontsize=16, fontweight="bold")

    # (1) L1 miss rate
    ax = axes[0, 0]
    for impl, rows_i in by_impl.items():
        xs, ys = series(rows_i, "num_vectors", "L1_miss_rate", drop_negative=True)
        ax.plot(xs, ys, marker="o", label=impl, color=colors.get(impl, None))
    ax.set_xscale("log")
    ax.set_xlabel("num_vectors (log)")
    ax.set_ylabel("L1 load miss rate (%)")
    ax.set_title("L1 Miss Rate")
    ax.grid(alpha=0.3)
    ax.legend()

    # (2) LLC miss rate
    ax = axes[0, 1]
    for impl, rows_i in by_impl.items():
        xs, ys = series(rows_i, "num_vectors", "LLC_miss_rate", drop_negative=True)
        ax.plot(xs, ys, marker="o", label=impl, color=colors.get(impl, None))
    ax.set_xscale("log")
    ax.set_xlabel("num_vectors (log)")
    ax.set_ylabel("LLC load miss rate (%)")
    ax.set_title("LLC Miss Rate")
    ax.grid(alpha=0.3)
    ax.legend()

    # (3) Cache miss rate
    ax = axes[1, 0]
    for impl, rows_i in by_impl.items():
        xs, ys = series(rows_i, "num_vectors", "cache_miss_rate", drop_negative=True)
        ax.plot(xs, ys, marker="o", label=impl, color=colors.get(impl, None))
    ax.set_xscale("log")
    ax.set_xlabel("num_vectors (log)")
    ax.set_ylabel("cache miss rate (%)")
    ax.set_title("Cache Miss Rate")
    ax.grid(alpha=0.3)
    ax.legend()

    # (4) IPC
    ax = axes[1, 1]
    for impl, rows_i in by_impl.items():
        xs, ys = series(rows_i, "num_vectors", "ipc", drop_negative=True)
        ax.plot(xs, ys, marker="o", label=impl, color=colors.get(impl, None))
    ax.set_xscale("log")
    ax.set_xlabel("num_vectors (log)")
    ax.set_ylabel("IPC (instructions/cycle)")
    ax.set_title("IPC")
    ax.grid(alpha=0.3)
    ax.legend()

    out_dir.mkdir(exist_ok=True)
    out_path = out_dir / f"rotation_cache_hierarchy_{op}.png"
    plt.tight_layout()
    plt.savefig(out_path, dpi=150, bbox_inches="tight")
    plt.close(fig)
    print(f"Saved: {out_path}")


def main():
    repo = Path(__file__).resolve().parents[2]
    csv_path = repo / "tmp" / "rotation_cache_hierarchy.csv"
    out_dir = get_plots_dir(repo / "plots")
    out_dir.mkdir(parents=True, exist_ok=True)

    if not csv_path.exists():
        raise SystemExit(f"CSV not found: {csv_path}")

    rows = load_rows(csv_path)
    if not rows:
        # Header-only CSV (perf unavailable) -> remove stale plots from previous runs.
        for op in ("rotation", "normalize"):
            p = out_dir / f"rotation_cache_hierarchy_{op}.png"
            if p.exists():
                p.unlink()
        return

    for op in sorted({r["op"] for r in rows}):
        plot_op(rows, op, out_dir)


if __name__ == "__main__":
    main()


