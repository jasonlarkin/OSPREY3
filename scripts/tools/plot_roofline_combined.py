#!/usr/bin/env python3
"""
Generate combined roofline plot with both memory-bound and compute-bound workloads.
"""

import csv
import sys
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np

def get_experiment_label():
    import os
    return (os.environ.get("OSPREY_EXPERIMENT_LABEL") or "").strip()

def get_plots_dir(base_plots_dir: Path) -> Path:
    label = get_experiment_label()
    return (base_plots_dir / label) if label else base_plots_dir

def load_empirical_results(csv_file):
    """Load empirical arithmetic intensity measurements."""
    stats = {}
    
    with open(csv_file, 'r') as reader:
        for row in reader:
            if 'version' in row.lower():
                continue  # Skip header
            parts = row.strip().split(',')
            if len(parts) >= 10:
                version = parts[0]
                system_size = parts[1]
                key = (version, system_size)
                gflops = float(parts[9])
                ai = float(parts[8])
                if gflops <= 0.0 or ai <= 0.0:
                    continue

                stats[key] = {
                    'gflops': gflops,
                    'arithmetic_intensity': ai,
                    'workload': 'pair_computation',
                }
    
    return stats

def plot_combined_roofline(pair_stats, compute_stats, output_dir, 
                           cpu_peak_flops=100.0, memory_bandwidth_gb_s=50.0):
    """
    Generate combined roofline plot with both workloads.
    """
    fig, ax = plt.subplots(figsize=(14, 10))
    
    # Calculate roofline
    ai_range = np.logspace(-2, 2, 1000)  # 0.01 to 100 FLOPs/byte
    
    # Memory bound: performance = AI * bandwidth
    memory_bound = ai_range * memory_bandwidth_gb_s
    
    # Compute bound: performance = peak flops (constant)
    compute_bound = np.full_like(ai_range, cpu_peak_flops)
    
    # Roofline is minimum of the two
    roofline = np.minimum(memory_bound, compute_bound)
    
    # Plot roofline
    ax.loglog(ai_range, roofline, 'k-', linewidth=3, label='Roofline Model', zorder=1)
    
    # Mark compute bound region
    compute_knee = cpu_peak_flops / memory_bandwidth_gb_s
    ax.axvline(x=compute_knee, color='r', linestyle='--', alpha=0.7, linewidth=2,
               label=f'Compute/Memory Boundary ({compute_knee:.2f} FLOPs/byte)', zorder=2)
    ax.axhline(y=cpu_peak_flops, color='g', linestyle='--', alpha=0.7, linewidth=2,
               label=f'Peak Compute ({cpu_peak_flops:.1f} GFLOP/s)', zorder=2)
    
    # Plot pair computation data (memory-bound)
    colors = {'scalar': '#e74c3c', 'avx2': '#2ecc71', 'avx512': '#3498db'}
    markers = {'scalar': 'o', 'avx2': 's', 'avx512': '^'}
    
    versions = ['scalar', 'avx2', 'avx512']
    for version in versions:
        x_vals = []
        y_vals = []
        labels = []
        
        for key, data in pair_stats.items():
            if key[0] == version:
                x_vals.append(data['arithmetic_intensity'])
                y_vals.append(data['gflops'])
                labels.append(f"{key[1]}")
        
        if x_vals:
            ax.scatter(x_vals, y_vals, c=colors[version], marker=markers[version],
                      s=300, alpha=0.8, label=f'{version.upper()} (Pair Computation)',
                      edgecolors='black', linewidth=2, zorder=3)
            
            # Add labels
            for x, y, label in zip(x_vals, y_vals, labels):
                ax.annotate(label, (x, y), xytext=(8, 8), textcoords='offset points',
                           fontsize=9, alpha=0.8, fontweight='bold',
                           bbox=dict(boxstyle='round,pad=0.3', facecolor='white', alpha=0.7))
    
    # Plot compute-bound data (matrix multiply)
    if compute_stats:
        compute_colors = {'scalar': '#c0392b', 'avx2': '#27ae60', 'avx512': '#2980b9'}
        compute_markers = {'scalar': 'D', 'avx2': 'p', 'avx512': 'H'}
        
        for version in versions:
            if version in compute_stats:
                data = compute_stats[version]
                ax.scatter([data['arithmetic_intensity']], [data['gflops']],
                          c=compute_colors[version], marker=compute_markers[version],
                          s=400, alpha=0.9, 
                          label=f'{version.upper()} (Matrix Multiply)',
                          edgecolors='black', linewidth=2, zorder=4)
                
                # Add label
                ax.annotate('Matrix\nMultiply', 
                           (data['arithmetic_intensity'], data['gflops']),
                           xytext=(15, 15), textcoords='offset points',
                           fontsize=10, alpha=0.9, fontweight='bold',
                           bbox=dict(boxstyle='round,pad=0.5', facecolor='yellow', alpha=0.8),
                           arrowprops=dict(arrowstyle='->', lw=1.5, alpha=0.7))
    
    ax.set_xlabel('Arithmetic Intensity (FLOPs/byte)', fontsize=14, fontweight='bold')
    ax.set_ylabel('Performance (GFLOP/s)', fontsize=14, fontweight='bold')
    label = get_experiment_label()
    title = 'Roofline Model: Memory-Bound vs Compute-Bound Workloads'
    if label:
        title += f' [{label}]'
    ax.set_title(title, fontsize=16, fontweight='bold')
    ax.legend(fontsize=11, loc='lower right', framealpha=0.9)
    ax.grid(alpha=0.3, linestyle='--', which='both', zorder=0)
    ax.set_xlim([0.001, 100])
    ax.set_ylim([0.1, cpu_peak_flops * 1.5])
    
    # Add text annotations
    ax.text(0.005, 20, 'Memory-Bound Region\n(Pair Computation)', 
            fontsize=12, fontweight='bold', color='#e74c3c',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', alpha=0.8),
            verticalalignment='bottom')
    
    ax.text(20, 80, 'Compute-Bound Region\n(Matrix Multiply)', 
            fontsize=12, fontweight='bold', color='#2980b9',
            bbox=dict(boxstyle='round,pad=0.5', facecolor='white', alpha=0.8),
            verticalalignment='top')
    
    plt.tight_layout()
    label = get_experiment_label()
    output_path = output_dir / 'roofline_plot_combined.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def main():
    # Load pair computation data
    pair_csv = Path(__file__).parent.parent.parent / 'arithmetic_intensity_measurements.csv'
    
    if not pair_csv.exists():
        print(f"Error: Pair computation CSV not found: {pair_csv}", file=sys.stderr)
        sys.exit(1)
    
    print(f"Loading pair computation data from: {pair_csv}")
    pair_stats = {}

    with open(pair_csv, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            try:
                gflops = float(row['measured_gflops'])
                ai = float(row['arithmetic_intensity'])
            except Exception:
                continue
            if gflops <= 0.0 or ai <= 0.0:
                continue
            key = (row['version'], row['system_size'])
            pair_stats[key] = {'gflops': gflops, 'arithmetic_intensity': ai}

    if not pair_stats:
        # Avoid leaving a stale plot around from previous runs.
        output_dir = get_plots_dir(pair_csv.parent / 'plots')
        output_path = output_dir / 'roofline_plot_combined.png'
        if output_path.exists():
            output_path.unlink()
        print("No valid empirical pair-computation points (perf likely unavailable); skipping combined roofline plot")
        sys.exit(0)
    
    # Add compute-bound data (from matrix multiply benchmark)
    # These are approximate values - you can update after running the benchmark
    compute_stats = {
        'scalar': {'gflops': 0.196, 'arithmetic_intensity': 42.67},
        'avx2': {'gflops': 0.797, 'arithmetic_intensity': 42.67},
        'avx512': {'gflops': 1.577, 'arithmetic_intensity': 42.67},
    }
    
    output_dir = get_plots_dir(pair_csv.parent / 'plots')
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print("Generating combined roofline plot...")
    plot_combined_roofline(pair_stats, compute_stats, output_dir, 
                          cpu_peak_flops=100.0, memory_bandwidth_gb_s=50.0)
    
    print(f"\nCombined roofline plot saved to: {output_dir}")

if __name__ == '__main__':
    main()

