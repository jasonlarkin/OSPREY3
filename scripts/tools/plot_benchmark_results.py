#!/usr/bin/env python3
"""
Generate plots from comprehensive benchmark results CSV.
"""

import csv
import sys
from pathlib import Path
from collections import defaultdict
import statistics
import matplotlib.pyplot as plt
import numpy as np

def system_size_sort_key(name: str) -> int:
    # Canonical ordering for preset system sizes used by the benchmarking scripts.
    order = {
        'small': 0,
        'medium': 1,
        'large': 2,
        'xlarge': 3,
        'xxlarge': 4,
    }
    return order.get(name, 999)

def get_experiment_label():
    # Set once for the whole process; scripts/tools/run_all_simd_analysis.sh exports this.
    import os
    return (os.environ.get("OSPREY_EXPERIMENT_LABEL") or "").strip()

def get_plots_dir(base_plots_dir: Path) -> Path:
    """
    If OSPREY_EXPERIMENT_LABEL is set, write plots into plots/<label>/ to avoid clobbering.
    """
    label = get_experiment_label()
    return (base_plots_dir / label) if label else base_plots_dir

def load_results(csv_file):
    """Load benchmark results from CSV."""
    results = defaultdict(list)  # key: (version, system_size) -> list of times
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row['version'], row['system_size'])
            time_us = float(row['time_us_per_iter'])
            results[key].append(time_us)
    
    return results

def calculate_statistics(results):
    """Calculate mean, stddev for each (version, system_size)."""
    stats = {}
    for key, times in results.items():
        stats[key] = {
            'n': len(times),
            'mean': statistics.mean(times),
            'median': statistics.median(times),
            'stddev': statistics.stdev(times) if len(times) > 1 else 0.0,
            'min': min(times),
            'max': max(times),
        }
    return stats

def calculate_speedups(stats):
    """Calculate speedup vs scalar for each system size."""
    systems = sorted(set(s for _, s in stats.keys()), key=system_size_sort_key)
    speedups = {'avx2': {}, 'avx512': {}}
    
    for system in systems:
        scalar_key = ('scalar', system)
        avx2_key = ('avx2', system)
        avx512_key = ('avx512', system)
        
        if scalar_key in stats:
            scalar_mean = stats[scalar_key]['mean']
            
            if avx2_key in stats:
                avx2_mean = stats[avx2_key]['mean']
                speedups['avx2'][system] = scalar_mean / avx2_mean
            
            if avx512_key in stats:
                avx512_mean = stats[avx512_key]['mean']
                speedups['avx512'][system] = scalar_mean / avx512_mean
    
    return speedups

def plot_speedup_comparison(speedups, output_dir):
    """Plot speedup comparison bar chart."""
    systems = sorted(speedups['avx2'].keys(), key=system_size_sort_key)
    avx2_values = [speedups['avx2'][s] for s in systems]
    avx512_values = [speedups['avx512'][s] for s in systems]
    
    x = np.arange(len(systems))
    width = 0.35
    
    fig, ax = plt.subplots(figsize=(10, 6))
    bars1 = ax.bar(x - width/2, avx2_values, width, label='AVX2', color='#2ecc71')
    bars2 = ax.bar(x + width/2, avx512_values, width, label='AVX-512', color='#3498db')
    
    ax.set_xlabel('System Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Speedup vs Scalar', fontsize=12, fontweight='bold')
    ax.set_title('SIMD Speedup Comparison by System Size', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(systems, fontsize=10)
    ax.legend(fontsize=11)
    ax.grid(axis='y', alpha=0.3, linestyle='--')
    ax.axhline(y=1.0, color='r', linestyle=':', alpha=0.5, label='No speedup')
    ax.set_ylim([0, max(max(avx2_values), max(avx512_values)) * 1.15])
    
    # Add value labels on bars
    for bars in [bars1, bars2]:
        for bar in bars:
            height = bar.get_height()
            ax.text(bar.get_x() + bar.get_width()/2., height,
                   f'{height:.2f}x',
                   ha='center', va='bottom', fontsize=9)
    
    plt.tight_layout()
    output_path = output_dir / 'speedup_comparison.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def plot_performance_comparison(stats, output_dir):
    """Plot performance comparison (time) across system sizes."""
    systems = sorted(set(s for _, s in stats.keys()), key=system_size_sort_key)
    versions = ['scalar', 'avx2', 'avx512']
    
    fig, ax = plt.subplots(figsize=(12, 7))
    
    x = np.arange(len(systems))
    width = 0.25
    
    colors = {'scalar': '#e74c3c', 'avx2': '#2ecc71', 'avx512': '#3498db'}
    
    for i, version in enumerate(versions):
        means = []
        stddevs = []
        for system in systems:
            key = (version, system)
            if key in stats:
                means.append(stats[key]['mean'])
                stddevs.append(stats[key]['stddev'])
            else:
                means.append(0)
                stddevs.append(0)
        
        offset = (i - 1) * width
        bars = ax.bar(x + offset, means, width, label=version.upper(),
                     color=colors[version], alpha=0.8, yerr=stddevs,
                     capsize=4, error_kw={'elinewidth': 1.5})
        
        # Add value labels
        for j, (bar, mean) in enumerate(zip(bars, means)):
            if mean > 0:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2., height + stddevs[j],
                       f'{mean:.1f}',
                       ha='center', va='bottom', fontsize=8)
    
    ax.set_xlabel('System Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Time per Iteration (μs)', fontsize=12, fontweight='bold')
    ax.set_title('Performance Comparison: Time per Iteration', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(systems, fontsize=10)
    ax.legend(fontsize=11, loc='upper left')
    ax.grid(axis='y', alpha=0.3, linestyle='--')
    ax.set_yscale('log')  # Log scale for better visualization
    
    plt.tight_layout()
    output_path = output_dir / 'performance_comparison.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def plot_variance_analysis(stats, output_dir):
    """Plot coefficient of variation (stddev/mean) to show variance."""
    systems = sorted(set(s for _, s in stats.keys()), key=system_size_sort_key)
    versions = ['scalar', 'avx2', 'avx512']
    
    fig, ax = plt.subplots(figsize=(12, 7))
    
    x = np.arange(len(systems))
    width = 0.25
    
    colors = {'scalar': '#e74c3c', 'avx2': '#2ecc71', 'avx512': '#3498db'}

    # If everything was produced with n=1, the coefficient of variation is always 0 by construction.
    # In that case, show an explicit note instead of an empty plot.
    min_n = min((stats.get((v, s), {}).get('n', 0) for v in versions for s in systems), default=0)
    
    for i, version in enumerate(versions):
        coeff_vars = []
        for system in systems:
            key = (version, system)
            if key in stats:
                mean = stats[key]['mean']
                stddev = stats[key]['stddev']
                coeff_var = (stddev / mean) * 100 if mean > 0 else 0
                coeff_vars.append(coeff_var)
            else:
                coeff_vars.append(0)
        
        offset = (i - 1) * width
        bars = ax.bar(x + offset, coeff_vars, width, label=version.upper(),
                     color=colors[version], alpha=0.8)
        
        # Add value labels
        for bar, cv in zip(bars, coeff_vars):
            if cv > 0:
                height = bar.get_height()
                ax.text(bar.get_x() + bar.get_width()/2., height,
                       f'{cv:.1f}%',
                       ha='center', va='bottom', fontsize=8)
    
    ax.set_xlabel('System Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Coefficient of Variation (%)', fontsize=12, fontweight='bold')
    ax.set_title('Variance Analysis: Coefficient of Variation', fontsize=14, fontweight='bold')
    ax.set_xticks(x)
    ax.set_xticklabels(systems, fontsize=10)
    ax.legend(fontsize=11)
    ax.grid(axis='y', alpha=0.3, linestyle='--')

    if min_n < 2:
        ax.text(
            0.5, 0.5,
            'Variance requires >=2 runs per config\n(current: n=1 => CV=0)',
            transform=ax.transAxes,
            ha='center', va='center',
            fontsize=12, fontweight='bold',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', alpha=0.85),
        )
    
    plt.tight_layout()
    output_path = output_dir / 'variance_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def plot_speedup_by_size(speedups, output_dir):
    """Plot speedup trend across system sizes."""
    systems = sorted(speedups['avx2'].keys(), key=system_size_sort_key)
    avx2_values = [speedups['avx2'][s] for s in systems]
    avx512_values = [speedups['avx512'][s] for s in systems]
    
    fig, ax = plt.subplots(figsize=(10, 6))
    
    ax.plot(systems, avx2_values, marker='o', linewidth=2, markersize=8,
           label='AVX2', color='#2ecc71')
    ax.plot(systems, avx512_values, marker='s', linewidth=2, markersize=8,
           label='AVX-512', color='#3498db')
    ax.axhline(y=1.0, color='r', linestyle='--', alpha=0.5, label='No speedup')
    
    ax.set_xlabel('System Size', fontsize=12, fontweight='bold')
    ax.set_ylabel('Speedup vs Scalar', fontsize=12, fontweight='bold')
    ax.set_title('SIMD Speedup Trend Across System Sizes', fontsize=14, fontweight='bold')
    ax.legend(fontsize=11, loc='best')
    ax.grid(alpha=0.3, linestyle='--')
    
    # Add value labels
    for i, (system, avx2_val, avx512_val) in enumerate(zip(systems, avx2_values, avx512_values)):
        ax.text(i, avx2_val + 0.02, f'{avx2_val:.2f}x', ha='center', va='bottom', fontsize=8, color='#2ecc71')
        ax.text(i, avx512_val + 0.02, f'{avx512_val:.2f}x', ha='center', va='bottom', fontsize=8, color='#3498db')
    
    plt.tight_layout()
    output_path = output_dir / 'speedup_trend.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def load_arithmetic_intensity_data():
    """Load empirical arithmetic intensity data if available."""
    ai_csv = Path(__file__).parent.parent.parent / 'arithmetic_intensity_measurements.csv'
    ai_data = {}
    
    if ai_csv.exists():
        with open(ai_csv, 'r') as f:
            reader = csv.DictReader(f)
            for row in reader:
                key = (row['version'], row['system_size'])
                try:
                    ai = float(row.get('arithmetic_intensity', '0') or 0.0)
                    gflops = float(row.get('measured_gflops', '0') or 0.0)
                except Exception:
                    continue

                # Skip missing/sentinel values (e.g. -1 when perf is unavailable)
                if ai <= 0.0 or gflops <= 0.0:
                    continue

                ai_data[key] = {
                    'arithmetic_intensity': ai,
                    'measured_gflops': gflops,
                    'flops_total': int(float(row.get('flops_total', '0') or 0.0)),
                    'bytes_read': int(float(row.get('bytes_read', '0') or 0.0)),
                    'bytes_written': int(float(row.get('bytes_written', '0') or 0.0)),
                }
    
    return ai_data

def plot_arithmetic_intensity_comparison(ai_data, output_dir):
    """Plot arithmetic intensity comparison across versions."""
    if not ai_data:
        return
    
    versions = ['scalar', 'avx2', 'avx512']
    systems = sorted(set(s for _, s in ai_data.keys()), key=system_size_sort_key)
    colors = {'scalar': '#e74c3c', 'avx2': '#2ecc71', 'avx512': '#3498db'}
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(14, 6))
    
    # Plot 1: Arithmetic Intensity
    x = np.arange(len(systems))
    width = 0.25
    
    for i, version in enumerate(versions):
        values = [ai_data.get((version, s), {}).get('arithmetic_intensity', 0) for s in systems]
        bars = ax1.bar(x + i*width, values, width, label=version.upper(), 
                      color=colors[version], alpha=0.8, edgecolor='black', linewidth=1.5)
        for bar, val in zip(bars, values):
            if val > 0:
                ax1.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                        f'{val:.4f}', ha='center', va='bottom', fontsize=8, fontweight='bold')
    
    ax1.set_xlabel('System Size', fontweight='bold', fontsize=11)
    ax1.set_ylabel('Arithmetic Intensity (FLOPs/byte)', fontweight='bold', fontsize=11)
    ax1.set_title('Arithmetic Intensity by Version', fontweight='bold', fontsize=13)
    ax1.set_xticks(x + width)
    ax1.set_xticklabels(systems)
    ax1.legend(fontsize=10)
    ax1.grid(axis='y', alpha=0.3, linestyle='--')
    ax1.axhline(y=2.0, color='r', linestyle='--', alpha=0.5, linewidth=2, label='Compute/Memory Boundary')
    ax1.text(0.02, 0.98, 'Memory-Bound Region\n(AI < 2.0)', transform=ax1.transAxes,
            fontsize=9, verticalalignment='top', bbox=dict(boxstyle='round', facecolor='yellow', alpha=0.3))
    
    # Plot 2: Measured GFLOP/s
    for i, version in enumerate(versions):
        values = [ai_data.get((version, s), {}).get('measured_gflops', 0) for s in systems]
        bars = ax2.bar(x + i*width, values, width, label=version.upper(),
                      color=colors[version], alpha=0.8, edgecolor='black', linewidth=1.5)
        for bar, val in zip(bars, values):
            if val > 0:
                ax2.text(bar.get_x() + bar.get_width()/2., bar.get_height(),
                        f'{val:.1f}', ha='center', va='bottom', fontsize=8, fontweight='bold')
    
    ax2.set_xlabel('System Size', fontweight='bold', fontsize=11)
    ax2.set_ylabel('Measured GFLOP/s', fontweight='bold', fontsize=11)
    ax2.set_title('Measured Performance (GFLOP/s)', fontweight='bold', fontsize=13)
    ax2.set_xticks(x + width)
    ax2.set_xticklabels(systems)
    ax2.legend(fontsize=10)
    ax2.grid(axis='y', alpha=0.3, linestyle='--')
    
    plt.tight_layout()
    output_path = output_dir / 'arithmetic_intensity_comparison.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def main():
    if len(sys.argv) < 2:
        csv_file = Path(__file__).parent.parent.parent / 'benchmark_results.csv'
    else:
        csv_file = Path(sys.argv[1])
    
    if not csv_file.exists():
        print(f"Error: CSV file not found: {csv_file}", file=sys.stderr)
        sys.exit(1)
    
    output_dir = get_plots_dir(csv_file.parent / 'plots')
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Loading results from: {csv_file}")
    results = load_results(csv_file)
    
    print("Loading arithmetic intensity data...")
    ai_data = load_arithmetic_intensity_data()
    if ai_data:
        print(f"  Found arithmetic intensity data for {len(ai_data)} configurations")
    else:
        print("  No arithmetic intensity data found (arithmetic_intensity_measurements.csv)")
        # Avoid leaving stale output behind (e.g., when perf becomes unavailable).
        stale = output_dir / 'arithmetic_intensity_comparison.png'
        if stale.exists():
            stale.unlink()
    
    print("Calculating statistics...")
    stats = calculate_statistics(results)
    
    print("Calculating speedups...")
    speedups = calculate_speedups(stats)
    
    print("\nGenerating plots...")
    plot_speedup_comparison(speedups, output_dir)
    plot_performance_comparison(stats, output_dir)
    plot_variance_analysis(stats, output_dir)
    plot_speedup_by_size(speedups, output_dir)
    
    if ai_data:
        plot_arithmetic_intensity_comparison(ai_data, output_dir)
    
    print(f"\nAll plots saved to: {output_dir}")

if __name__ == '__main__':
    main()

