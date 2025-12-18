#!/usr/bin/env python3
"""
Plot cache hierarchy analysis across system sizes.
Focus on medium system size to identify crossover points.
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

def load_cache_data(csv_file):
    """Load cache hierarchy data from CSV."""
    data = []
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            data.append({
                'version': row['version'],
                'l1_loads': int(row['L1_loads']),
                'l1_load_misses': int(row['L1_load_misses']),
                'l1_miss_rate': float(row['L1_miss_rate']),
                'l1_stores': int(row['L1_stores']),
                'l1_store_misses': int(row['L1_store_misses']),
                'llc_loads': int(row['LLC_loads']),
                'llc_load_misses': int(row['LLC_load_misses']),
                'llc_miss_rate': float(row['LLC_miss_rate']),
                'cache_misses': int(row['cache_misses']),
                'cache_references': int(row['cache_references']),
                'cache_miss_rate': float(row['cache_miss_rate']),
            })
    return data

def is_missing(x):
    return x is None or x < 0

def plot_cache_hierarchy(data, system_size, output_dir):
    """Plot cache hierarchy metrics."""
    label = get_experiment_label()
    versions = ['scalar', 'avx2', 'avx512']
    colors = {'scalar': '#e74c3c', 'avx2': '#2ecc71', 'avx512': '#3498db'}
    
    # Create subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    title = f'Cache Hierarchy Analysis: {system_size.upper()} System'
    if label:
        title += f' [{label}]'
    fig.suptitle(title, fontsize=16, fontweight='bold')
    
    # Extract data by version
    version_data = {v: next((d for d in data if d['version'] == v), None) for v in versions}
    
    # Plot 1: L1 Miss Rate
    ax1 = axes[0, 0]
    x_pos = np.arange(len(versions))
    l1_miss_rates = []
    l1_supported = False
    for v in versions:
        if not version_data[v]:
            l1_miss_rates.append(0.0)
            continue
        r = version_data[v]['l1_miss_rate']
        if not is_missing(r):
            l1_supported = True
            l1_miss_rates.append(r)
        else:
            l1_miss_rates.append(0.0)
    bars1 = ax1.bar(x_pos, l1_miss_rates, color=[colors[v] for v in versions], alpha=0.7, edgecolor='black')
    ax1.set_xlabel('Version', fontweight='bold')
    ax1.set_ylabel('L1 Miss Rate (%)', fontweight='bold')
    ax1.set_title('L1 Data Cache Miss Rate', fontweight='bold')
    ax1.set_xticks(x_pos)
    ax1.set_xticklabels([v.upper() for v in versions])
    ax1.grid(alpha=0.3, axis='y')
    for i, (bar, rate) in enumerate(zip(bars1, l1_miss_rates)):
        height = bar.get_height()
        label_txt = 'n/a' if (version_data[versions[i]] and is_missing(version_data[versions[i]]['l1_miss_rate'])) else f'{rate:.2f}%'
        ax1.text(bar.get_x() + bar.get_width()/2., height,
                label_txt, ha='center', va='bottom', fontweight='bold')
    if not l1_supported:
        ax1.text(0.5, 0.5, 'L1 events unsupported\n(perf unavailable or counters not supported)',
                 transform=ax1.transAxes, ha='center', va='center',
                 fontsize=10, fontweight='bold',
                 bbox=dict(boxstyle='round,pad=0.4', facecolor='white', alpha=0.8))
    
    # Plot 2: LLC Miss Rate (omit if unsupported)
    ax2 = axes[0, 1]
    llc_miss_rates = []
    llc_supported = False
    for v in versions:
        if not version_data[v]:
            llc_miss_rates.append(0.0)
            continue
        r = version_data[v]['llc_miss_rate']
        if not is_missing(r):
            llc_supported = True
            llc_miss_rates.append(r)
        else:
            llc_miss_rates.append(0.0)

    bars2 = ax2.bar(x_pos, llc_miss_rates, color=[colors[v] for v in versions], alpha=0.7, edgecolor='black')
    ax2.set_xlabel('Version', fontweight='bold')
    ax2.set_ylabel('LLC Miss Rate (%)', fontweight='bold')
    ax2.set_title('Last Level Cache (L3) Miss Rate', fontweight='bold')
    ax2.set_xticks(x_pos)
    ax2.set_xticklabels([v.upper() for v in versions])
    ax2.grid(alpha=0.3, axis='y')
    for i, (bar, rate) in enumerate(zip(bars2, llc_miss_rates)):
        height = bar.get_height()
        label = 'n/a' if (version_data[versions[i]] and is_missing(version_data[versions[i]]['llc_miss_rate'])) else f'{rate:.2f}%'
        ax2.text(bar.get_x() + bar.get_width()/2., height,
                label, ha='center', va='bottom', fontweight='bold')
    if not llc_supported:
        ax2.text(0.5, 0.5, 'LLC events unsupported\n(perf reports <not supported>)',
                 transform=ax2.transAxes, ha='center', va='center',
                 fontsize=10, fontweight='bold',
                 bbox=dict(boxstyle='round,pad=0.4', facecolor='white', alpha=0.8))
    
    # Plot 3: Cache Loads Comparison (omit LLC loads if unsupported)
    ax3 = axes[1, 0]
    l1_loads = [version_data[v]['l1_loads'] if version_data[v] else 0 for v in versions]
    llc_loads = []
    llc_loads_supported = False
    for v in versions:
        if not version_data[v]:
            llc_loads.append(0)
            continue
        val = version_data[v]['llc_loads']
        if not is_missing(val):
            llc_loads_supported = True
            llc_loads.append(val)
        else:
            llc_loads.append(0)
    x = np.arange(len(versions))
    width = 0.35
    bars3a = ax3.bar(x - width/2, [l/1e9 for l in l1_loads], width, label='L1 Loads', color='#3498db', alpha=0.7, edgecolor='black')
    if llc_loads_supported:
        ax3.bar(x + width/2, [l/1e9 for l in llc_loads], width, label='LLC Loads', color='#e67e22', alpha=0.7, edgecolor='black')
    ax3.set_xlabel('Version', fontweight='bold')
    ax3.set_ylabel('Loads (Billions)', fontweight='bold')
    ax3.set_title('Cache Loads Comparison', fontweight='bold')
    ax3.set_xticks(x)
    ax3.set_xticklabels([v.upper() for v in versions])
    ax3.legend()
    ax3.grid(alpha=0.3, axis='y')
    
    # Plot 4: Memory Hierarchy Traffic (only meaningful if LLC is supported)
    ax4 = axes[1, 1]
    # Estimate: L1 hits = L1 loads - L1 misses
    # L2/L3 hits = L1 misses - LLC misses (approximate)
    # Main memory = LLC misses
    if llc_supported:
        l1_hits = [version_data[v]['l1_loads'] - version_data[v]['l1_load_misses'] if version_data[v] else 0 for v in versions]
        llc_hits = [version_data[v]['l1_load_misses'] - version_data[v]['llc_load_misses'] if version_data[v] else 0 for v in versions]
        main_mem = [version_data[v]['llc_load_misses'] if version_data[v] else 0 for v in versions]
    else:
        l1_hits = [0 for _ in versions]
        llc_hits = [0 for _ in versions]
        main_mem = [0 for _ in versions]
    
    x = np.arange(len(versions))
    width = 0.6
    bottom1 = np.zeros(len(versions))
    bottom2 = bottom1 + [h/1e9 for h in l1_hits]
    
    if llc_supported:
        ax4.bar(x, [h/1e9 for h in l1_hits], width, label='L1 Hits', color='#2ecc71', alpha=0.7, edgecolor='black')
        ax4.bar(x, [h/1e9 for h in llc_hits], width, bottom=[h/1e9 for h in l1_hits], label='L2/L3 Hits', color='#f39c12', alpha=0.7, edgecolor='black')
        ax4.bar(x, [m/1e9 for m in main_mem], width, bottom=[(l1_hits[i] + llc_hits[i])/1e9 for i in range(len(versions))], label='Main Memory', color='#e74c3c', alpha=0.7, edgecolor='black')
    else:
        ax4.text(0.5, 0.5, 'Traffic breakdown requires LLC counters',
                 transform=ax4.transAxes, ha='center', va='center',
                 fontsize=10, fontweight='bold',
                 bbox=dict(boxstyle='round,pad=0.4', facecolor='white', alpha=0.8))
    
    ax4.set_xlabel('Version', fontweight='bold')
    ax4.set_ylabel('Memory Traffic (Billions of loads)', fontweight='bold')
    ax4.set_title('Memory Hierarchy Traffic Breakdown', fontweight='bold')
    ax4.set_xticks(x)
    ax4.set_xticklabels([v.upper() for v in versions])
    ax4.legend()
    ax4.grid(alpha=0.3, axis='y')
    
    plt.tight_layout()
    output_path = output_dir / f'cache_hierarchy_{system_size}.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")

def main():
    if len(sys.argv) < 2:
        print("Usage: plot_cache_hierarchy.py <system_size> [cache_csv_file]")
        print("  system_size: small, medium, large, xlarge, xxlarge")
        sys.exit(1)
    
    system_size = sys.argv[1]
    
    if len(sys.argv) >= 3:
        csv_file = Path(sys.argv[2])
    else:
        csv_file = Path(__file__).parent.parent.parent / 'tmp' / f'cache_hierarchy_{system_size}.csv'
    
    if not csv_file.exists():
        print(f"Error: CSV file not found: {csv_file}", file=sys.stderr)
        print("Run analyze_cache_hierarchy.sh first to generate cache data.", file=sys.stderr)
        sys.exit(1)
    
    output_dir = get_plots_dir(csv_file.parent.parent / 'plots')
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Loading cache data from: {csv_file}")
    data = load_cache_data(csv_file)
    
    print("Generating cache hierarchy plots...")
    plot_cache_hierarchy(data, system_size, output_dir)
    
    print(f"\nCache hierarchy plots saved to: {output_dir}")

if __name__ == '__main__':
    main()

