#!/usr/bin/env python3
"""
Generate roofline plot from benchmark results.
Roofline model shows compute vs memory bounds.
"""

import csv
import sys
from pathlib import Path
from collections import defaultdict
import statistics
import matplotlib.pyplot as plt
import numpy as np

def system_size_sort_key(name: str) -> int:
    order = {
        'small': 0,
        'medium': 1,
        'large': 2,
        'xlarge': 3,
        'xxlarge': 4,
    }
    return order.get(name, 999)

def get_experiment_label():
    import os
    return (os.environ.get("OSPREY_EXPERIMENT_LABEL") or "").strip()

def get_plots_dir(base_plots_dir: Path) -> Path:
    label = get_experiment_label()
    return (base_plots_dir / label) if label else base_plots_dir

def load_results(csv_file):
    """Load benchmark results from CSV."""
    results = defaultdict(list)
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            key = (row['version'], row['system_size'])
            time_us = float(row['time_us_per_iter'])
            atoms = int(row['atoms'])
            amber_pairs = int(row['amber_pairs'])
            eef1_pairs = int(row['eef1_pairs'])
            results[key].append({
                'time_us': time_us,
                'atoms': atoms,
                'amber_pairs': amber_pairs,
                'eef1_pairs': eef1_pairs,
            })
    
    return results

def estimate_flops_per_pair_amber():
    """
    Estimate floating-point operations per Amber pair.
    
    For Amber pair:
    - Distance calculation: 9 flops (3 subtract, 3 multiply, 3 add)
    - sqrt: ~10 flops (iterative, approximate)
    - Electrostatics: 3 flops (1 multiply, 1 divide, potential inv_r2 calc)
    - VdW: ~20 flops (r6 = r2*r2*r2: 2 mult, r12 = r6*r6: 1 mult, 
                      inv_r6 = 1/r6: 1 div, inv_r12 = 1/r12: 1 div,
                      vdwA*inv_r12: 1 mult, vdwB*inv_r6: 1 mult, subtract: 1)
    Total: ~42 flops per Amber pair
    
    For EEF1 pair:
    - Distance: 9 flops
    - sqrt: ~10 flops
    - Xij, Xji calculations: 4 flops (2 subtracts, 2 divides)
    - exp(-Xij²), exp(-Xji²): ~50 flops each (exp is expensive - Taylor series or hardware)
    - Multiply by alpha: 2 flops
    - Add: 1 flop
    - Multiply by -1 and divide by r²: 2 flops
    Total: ~128 flops per EEF1 pair
    
    Weighted average (assuming 2:1 ratio of Amber:EEF1 pairs):
    (2 * 42 + 1 * 128) / 3 = ~71 flops per pair
    
    But we have varying ratios, so use conservative estimate:
    """
    return 42.0

def estimate_flops_per_pair_eef1():
    """
    Estimate floating-point operations per EEF1 pair.

    EEF1 is compute-heavy due to exp() terms.
    """
    return 128.0

def estimate_bytes_per_pair(atoms):
    """
    Estimate memory traffic per atom pair.
    
    SIMD considerations:
    - For AVX2: loading 4 pairs at once, but accessing atoms non-contiguously
    - For AVX-512: loading 8 pairs at once
    
    Reads:
    - 2 atoms (atom1, atom2): 2 * 3 * 8 bytes = 48 bytes
    - But with SIMD gather operations, cache line effects matter
    - Each atom might span cache lines (64 bytes per line)
    - Worst case: 2 cache lines per atom = 128 bytes
    - Pair parameters: ~32 bytes (Amber) or ~56 bytes (EEF1)
    - For SIMD, loading 4-8 pair parameters at once: ~32-56 bytes total
    
    Effective reads (considering cache effects):
    - Scalar: ~48 bytes (atoms) + 32 bytes (params) = 80 bytes
    - SIMD: More efficient loading, but gather overhead
    - Conservative estimate: ~64 bytes per pair (atoms can be in cache)
    
    Writes:
    - Minimal: accumulators in registers
    
    Total effective: ~64 bytes per pair (if atoms stay in L1 cache)
    """
    # More realistic: atoms fit in L1 cache, so repeated access is cheap
    # Only count unique memory traffic per pair
    return 64.0

def calculate_performance(results):
    """Calculate GFLOP/s and arithmetic intensity."""
    stats = {}
    
    for key, data_list in results.items():
        version, system_size = key
        # Use mean time
        times = [d['time_us'] for d in data_list]
        mean_time_us = statistics.mean(times)
        
        # Get system size from first entry
        atoms = data_list[0]['atoms']
        amber_pairs = data_list[0]['amber_pairs']
        eef1_pairs = data_list[0]['eef1_pairs']
        total_pairs = amber_pairs + eef1_pairs
        
        # Estimate FLOPs and bytes (vary AI based on Amber vs EEF1 mix)
        flops_amber = estimate_flops_per_pair_amber()
        flops_eef1 = estimate_flops_per_pair_eef1()
        bytes_per_pair = estimate_bytes_per_pair(atoms)

        total_flops = (amber_pairs * flops_amber) + (eef1_pairs * flops_eef1)
        total_bytes = total_pairs * bytes_per_pair
        
        # Convert time to seconds
        time_sec = mean_time_us * 1e-6
        
        # Calculate performance
        gflops = (total_flops / time_sec) / 1e9
        arithmetic_intensity = total_flops / total_bytes  # FLOPs/byte
        
        stats[key] = {
            'gflops': gflops,
            'arithmetic_intensity': arithmetic_intensity,
            'time_us': mean_time_us,
            'total_pairs': total_pairs,
        }
    
    return stats

def plot_roofline(stats, output_dir, cpu_peak_flops=100.0, memory_bandwidth_gb_s=50.0):
    """
    Generate roofline plot.
    
    Args:
        cpu_peak_flops: Peak compute performance in GFLOP/s
        memory_bandwidth_gb_s: Peak memory bandwidth in GB/s
    """
    fig, ax = plt.subplots(figsize=(12, 8))
    
    # Calculate roofline
    # For each arithmetic intensity, performance is min(compute_bound, memory_bound)
    # compute_bound = CPU_peak_flops (constant)
    # memory_bound = arithmetic_intensity * memory_bandwidth
    
    ai_range = np.logspace(-2, 2, 1000)  # 0.01 to 100 FLOPs/byte
    
    # Memory bound: performance = AI * bandwidth
    memory_bound = ai_range * memory_bandwidth_gb_s
    
    # Compute bound: performance = peak flops (constant)
    compute_bound = np.full_like(ai_range, cpu_peak_flops)
    
    # Roofline is minimum of the two
    roofline = np.minimum(memory_bound, compute_bound)
    
    # Plot roofline
    ax.loglog(ai_range, roofline, 'k-', linewidth=2, label='Roofline Model')
    
    # Mark compute bound region
    compute_knee = cpu_peak_flops / memory_bandwidth_gb_s
    ax.axvline(x=compute_knee, color='r', linestyle='--', alpha=0.5, label=f'Compute/Memory Boundary ({compute_knee:.2f} FLOPs/byte)')
    ax.axhline(y=cpu_peak_flops, color='g', linestyle='--', alpha=0.5, label=f'Peak Compute ({cpu_peak_flops:.1f} GFLOP/s)')
    
    # Plot actual data points
    colors = {'scalar': '#e74c3c', 'avx2': '#2ecc71', 'avx512': '#3498db'}
    markers = {'scalar': 'o', 'avx2': 's', 'avx512': '^'}
    
    systems = sorted(set(s for _, s in stats.keys()), key=system_size_sort_key)
    versions = ['scalar', 'avx2', 'avx512']
    
    for version in versions:
        x_vals = []
        y_vals = []
        labels = []
        
        for system in systems:
            key = (version, system)
            if key in stats:
                x_vals.append(stats[key]['arithmetic_intensity'])
                y_vals.append(stats[key]['gflops'])
                labels.append(system)
        
        if x_vals:
            ax.scatter(x_vals, y_vals, c=colors[version], marker=markers[version],
                      s=150, alpha=0.7, label=version.upper(), edgecolors='black', linewidth=1.5)
            
            # Add labels
            for x, y, label in zip(x_vals, y_vals, labels):
                ax.annotate(label, (x, y), xytext=(5, 5), textcoords='offset points',
                           fontsize=8, alpha=0.7)
    
    ax.set_xlabel('Arithmetic Intensity (FLOPs/byte)', fontsize=12, fontweight='bold')
    ax.set_ylabel('Performance (GFLOP/s)', fontsize=12, fontweight='bold')
    label = get_experiment_label()
    title = 'Roofline Model: SIMD Performance Analysis'
    if label:
        title += f' [{label}]'
    ax.set_title(title, fontsize=14, fontweight='bold')
    ax.legend(fontsize=10, loc='best')
    ax.grid(alpha=0.3, linestyle='--', which='both')
    ax.set_xlim([0.01, 100])
    ax.set_ylim([0.01, cpu_peak_flops * 1.5])
    
    plt.tight_layout()
    output_path = output_dir / 'roofline_plot.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"Saved: {output_path}")
    
    # Print statistics
    print("\n=== Roofline Analysis ===")
    print(f"Compute/Memory Boundary: {compute_knee:.2f} FLOPs/byte")
    print(f"  - AI > {compute_knee:.2f}: Compute-bound (limited by CPU)")
    print(f"  - AI < {compute_knee:.2f}: Memory-bound (limited by memory bandwidth)")
    print("")
    print(f"{'Version':<10} {'System':<10} {'AI (FLOPs/B)':<15} {'GFLOP/s':<12} {'Bound':<10}")
    print("-" * 65)
    
    for version in versions:
        for system in systems:
            key = (version, system)
            if key in stats:
                ai = stats[key]['arithmetic_intensity']
                gflops = stats[key]['gflops']
                bound = "compute" if ai > compute_knee else "memory"
                print(f"{version:<10} {system:<10} {ai:<15.3f} {gflops:<12.3f} {bound:<10}")

def load_empirical_results(csv_file):
    """Load empirical arithmetic intensity measurements."""
    stats = {}
    
    with open(csv_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            version = row['version']
            system_size = row['system_size']
            key = (version, system_size)

            # Treat non-positive or sentinel values as missing (e.g. -1 when perf is unavailable).
            gflops = float(row.get('measured_gflops', '0') or 0.0)
            ai = float(row.get('arithmetic_intensity', '0') or 0.0)
            if gflops <= 0.0 or ai <= 0.0:
                continue

            stats[key] = {
                'gflops': gflops,
                'arithmetic_intensity': ai,
                'flops_total': int(float(row.get('flops_total', '0') or 0.0)),
                'bytes_read': int(float(row.get('bytes_read', '0') or 0.0)),
                'bytes_written': int(float(row.get('bytes_written', '0') or 0.0)),
            }
    
    return stats

def main():
    if len(sys.argv) < 2:
        # Try empirical data first, fall back to benchmark results
        empirical_csv = Path(__file__).parent.parent.parent / 'arithmetic_intensity_measurements.csv'
        benchmark_csv = Path(__file__).parent.parent.parent / 'benchmark_results.csv'
        
        if empirical_csv.exists():
            csv_file = empirical_csv
            use_empirical = True
        elif benchmark_csv.exists():
            csv_file = benchmark_csv
            use_empirical = False
        else:
            print(f"Error: No CSV file found. Expected:", file=sys.stderr)
            print(f"  - {empirical_csv}", file=sys.stderr)
            print(f"  - {benchmark_csv}", file=sys.stderr)
            sys.exit(1)
    else:
        csv_file = Path(sys.argv[1])
        use_empirical = 'arithmetic_intensity' in csv_file.name
    
    if not csv_file.exists():
        print(f"Error: CSV file not found: {csv_file}", file=sys.stderr)
        sys.exit(1)
    
    output_dir = get_plots_dir(csv_file.parent / 'plots')
    output_dir.mkdir(parents=True, exist_ok=True)
    
    print(f"Loading results from: {csv_file}")
    
    if use_empirical:
        print("Using empirical arithmetic intensity data")
        stats = load_empirical_results(csv_file)
        if not stats:
            # Common on WSL when perf isn't installed/supported: the empirical CSV exists but contains only missing sentinels.
            benchmark_csv = Path(__file__).parent.parent.parent / 'benchmark_results.csv'
            if benchmark_csv.exists():
                print("Empirical data unavailable; falling back to benchmark timing estimates")
                results = load_results(benchmark_csv)
                print("Calculating performance metrics...")
                stats = calculate_performance(results)
                csv_file = benchmark_csv
            else:
                print("Empirical data unavailable and benchmark_results.csv missing; nothing to plot", file=sys.stderr)
                sys.exit(0)
    else:
        print("Using estimated arithmetic intensity from benchmark timing")
        results = load_results(csv_file)
        print("Calculating performance metrics...")
        stats = calculate_performance(results)
    
    print("Generating roofline plot...")
    # Estimate CPU specs (adjust based on your CPU)
    # For Intel i3-1115G4: ~100 GFLOP/s peak, ~50 GB/s memory bandwidth
    plot_roofline(stats, output_dir, cpu_peak_flops=100.0, memory_bandwidth_gb_s=50.0)
    
    print(f"\nRoofline plot saved to: {output_dir}")

if __name__ == '__main__':
    main()

