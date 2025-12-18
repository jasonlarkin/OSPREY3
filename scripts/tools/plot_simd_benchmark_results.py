#!/usr/bin/env python3
"""
Visualize SIMD benchmark results from parameter sweep

Usage:
    ./build/benchmark_simd_parameter_sweep 500 > results.csv
    python3 scripts/tools/plot_simd_benchmark_results.py results.csv
"""

import sys
import pandas as pd
import matplotlib.pyplot as plt
import numpy as np
from pathlib import Path
import os

def get_experiment_label():
    return (os.environ.get("OSPREY_EXPERIMENT_LABEL") or "").strip()

def get_plots_dir(base_plots_dir: Path) -> Path:
    label = get_experiment_label()
    return (base_plots_dir / label) if label else base_plots_dir

def parse_csv(filename):
    """Parse the CSV output from benchmark_simd_parameter_sweep"""
    data = []
    with open(filename, 'r') as f:
        for line in f:
            line = line.strip()
            if line.startswith('#') or not line:
                continue
            parts = line.split(',')
            # Format has 12 columns: num_atoms, num_amber_pairs, num_eef1_pairs, total_pairs,
            # mem_ops_mb, compute_intensity, scalar_us, avx2_us, avx512_us,
            # avx2_speedup, avx512_speedup, avx512_vs_avx2
            if len(parts) >= 12:
                try:
                    data.append({
                        'num_atoms': int(parts[0]),
                        'num_amber_pairs': int(parts[1]),
                        'num_eef1_pairs': int(parts[2]),
                        'total_pairs': int(parts[3]),
                        'mem_ops_mb': float(parts[4]),
                        'compute_intensity': float(parts[5]),
                        'scalar_us': float(parts[6]),
                        'avx2_us': float(parts[7]),
                        'avx512_us': float(parts[8]),
                        'avx2_speedup': float(parts[9]),
                        'avx512_speedup': float(parts[10]),
                        'avx512_vs_avx2': float(parts[11])
                    })
                except (ValueError, IndexError) as e:
                    print(f"Warning: Skipping malformed line: {line[:80]}... ({e})", file=sys.stderr)
                    continue
    return pd.DataFrame(data)

def plot_speedup_vs_total_pairs(df, output_dir):
    """Plot speedup vs total number of pairs"""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    
    # AVX2 speedup
    axes[0].scatter(df['total_pairs'], df['avx2_speedup'], alpha=0.6, s=50)
    axes[0].axhline(y=1.0, color='r', linestyle='--', label='Baseline (1.0x)')
    axes[0].set_xlabel('Total Pairs')
    axes[0].set_ylabel('AVX2 Speedup')
    axes[0].set_title('AVX2 Speedup vs Total Pairs')
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()
    axes[0].set_xscale('log')
    
    # AVX-512 speedup
    axes[1].scatter(df['total_pairs'], df['avx512_speedup'], alpha=0.6, s=50, color='green')
    axes[1].axhline(y=1.0, color='r', linestyle='--', label='Baseline (1.0x)')
    axes[1].set_xlabel('Total Pairs')
    axes[1].set_ylabel('AVX-512 Speedup')
    axes[1].set_title('AVX-512 Speedup vs Total Pairs')
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()
    axes[1].set_xscale('log')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'speedup_vs_total_pairs.png', dpi=150)
    print(f"Saved: {output_dir / 'speedup_vs_total_pairs.png'}")

def plot_speedup_vs_memory_ops(df, output_dir):
    """Plot speedup vs memory operations"""
    fig, axes = plt.subplots(1, 2, figsize=(14, 5))
    
    # AVX2
    axes[0].scatter(df['mem_ops_mb'], df['avx2_speedup'], alpha=0.6, s=50)
    axes[0].axhline(y=1.0, color='r', linestyle='--', label='Baseline (1.0x)')
    axes[0].set_xlabel('Memory Operations (MB)')
    axes[0].set_ylabel('AVX2 Speedup')
    axes[0].set_title('AVX2 Speedup vs Memory Operations')
    axes[0].grid(True, alpha=0.3)
    axes[0].legend()
    
    # AVX-512
    axes[1].scatter(df['mem_ops_mb'], df['avx512_speedup'], alpha=0.6, s=50, color='green')
    axes[1].axhline(y=1.0, color='r', linestyle='--', label='Baseline (1.0x)')
    axes[1].set_xlabel('Memory Operations (MB)')
    axes[1].set_ylabel('AVX-512 Speedup')
    axes[1].set_title('AVX-512 Speedup vs Memory Operations')
    axes[1].grid(True, alpha=0.3)
    axes[1].legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'speedup_vs_memory_ops.png', dpi=150)
    print(f"Saved: {output_dir / 'speedup_vs_memory_ops.png'}")

def plot_roofline(df, output_dir):
    """Create a roofline-style plot: Performance vs Compute Intensity"""
    # Estimate peak performance (FLOPs/sec)
    # Rough estimate: 1 GHz × 8 doubles (AVX-512) × 2 FMA units = ~16 GFLOP/s per core
    peak_compute_avx512 = 16e9  # GFLOP/s (rough estimate)
    peak_compute_avx2 = 8e9     # GFLOP/s
    peak_compute_scalar = 2e9   # GFLOP/s
    
    # Estimate peak memory bandwidth (GB/s)
    peak_memory_bw = 50e9  # GB/s (DDR4 typical)
    
    # Calculate performance for each data point
    # Performance = (total_pairs * flops_per_pair) / time
    # time_us is in microseconds, convert to seconds: time_us * 1e-6
    # GFLOP/s = (flops) / (time_seconds) * 1e-9 = (flops) / (time_us * 1e-6) * 1e-9 = flops / time_us * 1e-3
    flops_per_pair = 25.0  # Rough estimate: ~15 arithmetic ops + 10 memory ops counted as compute
    df['gflops_scalar'] = (df['total_pairs'] * flops_per_pair) / df['scalar_us'] * 1e-3
    df['gflops_avx2'] = (df['total_pairs'] * flops_per_pair) / df['avx2_us'] * 1e-3
    df['gflops_avx512'] = (df['total_pairs'] * flops_per_pair) / df['avx512_us'] * 1e-3
    
    # Compute intensity = FLOPs / Byte
    df['compute_intensity_actual'] = df['compute_intensity']
    
    fig, ax = plt.subplots(1, 1, figsize=(10, 8))
    
    # Plot roofline ceilings
    intensity_range = np.logspace(-2, 2, 100)
    compute_ceiling = np.minimum(peak_compute_avx512 / 1e9, intensity_range * peak_memory_bw / 1e9)
    ax.plot(intensity_range, compute_ceiling, 'k--', linewidth=2, label='Roofline (AVX-512)')
    
    # Plot actual performance points
    ax.scatter(df['compute_intensity_actual'], df['gflops_scalar'], 
              alpha=0.6, s=50, label='Scalar', color='red')
    ax.scatter(df['compute_intensity_actual'], df['gflops_avx2'], 
              alpha=0.6, s=50, label='AVX2', color='blue')
    ax.scatter(df['compute_intensity_actual'], df['gflops_avx512'], 
              alpha=0.6, s=50, label='AVX-512', color='green')
    
    ax.set_xlabel('Compute Intensity (FLOPs/Byte)')
    ax.set_ylabel('Performance (GFLOP/s)')
    ax.set_title('Roofline Plot: Performance vs Compute Intensity')
    ax.set_xscale('log')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3)
    ax.legend()
    
    plt.tight_layout()
    plt.savefig(output_dir / 'roofline_plot.png', dpi=150)
    print(f"Saved: {output_dir / 'roofline_plot.png'}")

def plot_heatmap_speedup(df, output_dir):
    """Create heatmap of speedup across different parameter combinations"""
    # Aggregate by total pairs and compute intensity
    df['pairs_bin'] = pd.cut(df['total_pairs'], bins=10, labels=False)
    df['intensity_bin'] = pd.cut(df['compute_intensity'], bins=10, labels=False)
    
    pivot_avx2 = df.pivot_table(values='avx2_speedup', 
                                 index='pairs_bin', 
                                 columns='intensity_bin', 
                                 aggfunc='mean')
    pivot_avx512 = df.pivot_table(values='avx512_speedup', 
                                   index='pairs_bin', 
                                   columns='intensity_bin', 
                                   aggfunc='mean')
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    
    # AVX2 heatmap
    im1 = axes[0].imshow(pivot_avx2.values, cmap='RdYlGn', aspect='auto', vmin=0.5, vmax=2.0)
    axes[0].set_title('AVX2 Speedup Heatmap')
    axes[0].set_xlabel('Compute Intensity Bin')
    axes[0].set_ylabel('Total Pairs Bin')
    plt.colorbar(im1, ax=axes[0])
    
    # AVX-512 heatmap
    im2 = axes[1].imshow(pivot_avx512.values, cmap='RdYlGn', aspect='auto', vmin=0.5, vmax=2.0)
    axes[1].set_title('AVX-512 Speedup Heatmap')
    axes[1].set_xlabel('Compute Intensity Bin')
    axes[1].set_ylabel('Total Pairs Bin')
    plt.colorbar(im2, ax=axes[1])
    
    plt.tight_layout()
    plt.savefig(output_dir / 'speedup_heatmap.png', dpi=150)
    print(f"Saved: {output_dir / 'speedup_heatmap.png'}")

def plot_comparison_all(df, output_dir):
    """Compare all three implementations side by side"""
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Performance vs total pairs
    axes[0, 0].scatter(df['total_pairs'], df['scalar_us'], alpha=0.6, s=30, label='Scalar', color='red')
    axes[0, 0].scatter(df['total_pairs'], df['avx2_us'], alpha=0.6, s=30, label='AVX2', color='blue')
    axes[0, 0].scatter(df['total_pairs'], df['avx512_us'], alpha=0.6, s=30, label='AVX-512', color='green')
    axes[0, 0].set_xlabel('Total Pairs')
    axes[0, 0].set_ylabel('Time (μs)')
    axes[0, 0].set_title('Execution Time vs Total Pairs')
    axes[0, 0].set_xscale('log')
    axes[0, 0].set_yscale('log')
    axes[0, 0].grid(True, alpha=0.3)
    axes[0, 0].legend()
    
    # Speedup comparison
    axes[0, 1].scatter(df['total_pairs'], df['avx2_speedup'], alpha=0.6, s=30, label='AVX2', color='blue')
    axes[0, 1].scatter(df['total_pairs'], df['avx512_speedup'], alpha=0.6, s=30, label='AVX-512', color='green')
    axes[0, 1].axhline(y=1.0, color='r', linestyle='--', linewidth=1)
    axes[0, 1].set_xlabel('Total Pairs')
    axes[0, 1].set_ylabel('Speedup (vs Scalar)')
    axes[0, 1].set_title('Speedup vs Total Pairs')
    axes[0, 1].set_xscale('log')
    axes[0, 1].grid(True, alpha=0.3)
    axes[0, 1].legend()
    
    # AVX-512 vs AVX2
    axes[1, 0].scatter(df['total_pairs'], df['avx512_vs_avx2'], alpha=0.6, s=30, color='purple')
    axes[1, 0].axhline(y=1.0, color='r', linestyle='--', linewidth=1)
    axes[1, 0].set_xlabel('Total Pairs')
    axes[1, 0].set_ylabel('AVX-512 Speedup vs AVX2')
    axes[1, 0].set_title('AVX-512 vs AVX2 Direct Comparison')
    axes[1, 0].set_xscale('log')
    axes[1, 0].grid(True, alpha=0.3)
    
    # Summary statistics
    axes[1, 1].axis('off')
    stats_text = f"""
Summary Statistics:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
AVX2 Speedup:
  Mean: {df['avx2_speedup'].mean():.3f}x
  Median: {df['avx2_speedup'].median():.3f}x
  Min: {df['avx2_speedup'].min():.3f}x
  Max: {df['avx2_speedup'].max():.3f}x

AVX-512 Speedup:
  Mean: {df['avx512_speedup'].mean():.3f}x
  Median: {df['avx512_speedup'].median():.3f}x
  Min: {df['avx512_speedup'].min():.3f}x
  Max: {df['avx512_speedup'].max():.3f}x

AVX-512 vs AVX2:
  Mean: {df['avx512_vs_avx2'].mean():.3f}x
  Median: {df['avx512_vs_avx2'].median():.3f}x

Total Data Points: {len(df)}
"""
    axes[1, 1].text(0.1, 0.5, stats_text, fontfamily='monospace', 
                    fontsize=10, verticalalignment='center')
    
    plt.tight_layout()
    plt.savefig(output_dir / 'comparison_all.png', dpi=150)
    print(f"Saved: {output_dir / 'comparison_all.png'}")

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 plot_simd_benchmark_results.py <results.csv> [output_dir]")
        sys.exit(1)
    
    input_file = Path(sys.argv[1])
    if not input_file.exists():
        print(f"Error: File not found: {input_file}")
        sys.exit(1)
    
    # Default to plots directory in repo root (assuming script is in scripts/tools/).
    # If OSPREY_EXPERIMENT_LABEL is set, write into plots/<label>/ to avoid clobbering.
    if len(sys.argv) > 2:
        output_dir = Path(sys.argv[2])
    else:
        # Try to find repo root (look for .git, build.gradle.kts, or src/)
        script_dir = Path(__file__).parent
        repo_root = script_dir.parent.parent  # scripts/tools -> scripts -> repo root
        # Fallback to input file's directory if repo root not found
        if (repo_root / 'build.gradle.kts').exists() or (repo_root / '.git').exists():
            output_dir = get_plots_dir(repo_root / 'plots')
        else:
            output_dir = input_file.parent / 'plots'
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Reading data from: {input_file}")
    df = parse_csv(input_file)
    print(f"Loaded {len(df)} data points")
    
    print("\nGenerating plots...")
    plot_speedup_vs_total_pairs(df, output_dir)
    plot_speedup_vs_memory_ops(df, output_dir)
    plot_roofline(df, output_dir)
    plot_heatmap_speedup(df, output_dir)
    plot_comparison_all(df, output_dir)
    
    print(f"\nAll plots saved to: {output_dir}")
    print("\nTip: Use 'perf stat' to profile the benchmark for detailed analysis:")
    print("  perf stat -e cache-misses,LLC-load-misses ./build/benchmark_simd_direct 5000")

if __name__ == '__main__':
    main()

