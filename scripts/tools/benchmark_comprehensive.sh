#!/bin/bash
# Comprehensive SIMD benchmark script
# Tests multiple system sizes with proper iterations and statistics
# Based on BENCHMARKING_PLAN.md

set -e

BENCH_DIR="src/main/cc/ConfEcalc"
OUTPUT_CSV="benchmark_results.csv"
NUM_RUNS=${1:-5}  # Number of runs per configuration for statistics

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

echo "=== Comprehensive SIMD Benchmark ==="
echo "Multiple runs per config: $NUM_RUNS"
echo "Output CSV: $OUTPUT_CSV"
echo ""

# System size configurations (atoms, amber_pairs, eef1_pairs, iterations)
declare -a CONFIGS=(
    # Format: "name:atoms:amber_pairs:eef1_pairs:iterations"
    "small:200:2000:1000:50000"
    "medium:500:1000:500:20000"
    "large:1000:7500:500:10000"
    "xlarge:2000:15000:2000:5000"
    "xxlarge:5000:50000:5000:1000"
)

# Initialize CSV file
echo "version,system_size,atoms,amber_pairs,eef1_pairs,iterations,time_us_per_iter,time_total_ms,run_id" > "$OUTPUT_CSV"

# Function to run a single benchmark
run_single_benchmark() {
    local version=$1
    local system_size=$2
    local atoms=$3
    local amber_pairs=$4
    local eef1_pairs=$5
    local iterations=$6
    local run_id=$7
    
    local bench_bin="$REPO_ROOT/$BENCH_DIR/build/benchmark_${version}_only"
    
    if [ ! -f "$bench_bin" ]; then
        echo "  ERROR: Benchmark binary not found: $bench_bin"
        return 1
    fi
    
    # Run benchmark - output is just time_us_per_iter
    local time_us=$("$bench_bin" "$atoms" "$amber_pairs" "$eef1_pairs" "$iterations" 2>/dev/null | tail -1 | awk '{print $1}')
    
    if [ -z "$time_us" ] || [ "$time_us" = "0" ]; then
        echo "  ERROR: Failed to get timing"
        return 1
    fi
    
    # Calculate total time in ms
    local time_total_ms=$(python3 -c "print(f'{($time_us * $iterations) / 1000.0:.2f}')" 2>/dev/null)
    
    # Write to CSV
    echo "$version,$system_size,$atoms,$amber_pairs,$eef1_pairs,$iterations,$time_us,$time_total_ms,$run_id" >> "$OUTPUT_CSV"
    
    echo "$time_us us/iter"
}

# Benchmark a single configuration across all versions and multiple runs
benchmark_config() {
    local config=$1
    IFS=':' read -r name atoms amber_pairs eef1_pairs iterations <<< "$config"
    
    echo "=== System: $name ==="
    echo "  Atoms: $atoms, Amber pairs: $amber_pairs, EEF1 pairs: $eef1_pairs"
    echo "  Iterations: $iterations"
    echo ""
    
    for version in scalar avx2 avx512; do
        echo -n "  $version: "
        
        for run_id in $(seq 0 $((NUM_RUNS - 1))); do
            echo -n "[$((run_id + 1))/$NUM_RUNS] "
            run_single_benchmark "$version" "$name" "$atoms" "$amber_pairs" "$eef1_pairs" "$iterations" "$run_id"
        done
        
        echo ""
    done
    
    echo ""
}

# Benchmark all configurations
for config in "${CONFIGS[@]}"; do
    benchmark_config "$config"
done

echo "=== Benchmark Complete ==="
echo "Results saved to: $OUTPUT_CSV"
echo ""
echo "Generating statistics..."

# Generate statistics summary
python3 << PYEOF
import csv
import sys
from collections import defaultdict
import statistics

csv_file = "$OUTPUT_CSV"

results = defaultdict(list)  # key: (version, system_size) -> list of times

with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        key = (row['version'], row['system_size'])
        time_us = float(row['time_us_per_iter'])
        results[key].append(time_us)

print("\n=== Performance Summary ===")
print(f"{'Version':<10} {'System':<10} {'Mean (us)':<12} {'Median (us)':<12} {'StdDev (us)':<12} {'Min (us)':<12} {'Max (us)':<12}")
print("-" * 90)

for (version, system_size) in sorted(results.keys()):
    times = results[(version, system_size)]
    mean = statistics.mean(times)
    median = statistics.median(times)
    stdev = statistics.stdev(times) if len(times) > 1 else 0.0
    min_time = min(times)
    max_time = max(times)
    
    print(f"{version:<10} {system_size:<10} {mean:<12.2f} {median:<12.2f} {stdev:<12.2f} {min_time:<12.2f} {max_time:<12.2f}")

print("\n=== Speedup vs Scalar ===")
print(f"{'System':<10} {'AVX2 Speedup':<15} {'AVX-512 Speedup':<18}")
print("-" * 45)

systems = sorted(set(s for _, s in results.keys()))
for system in systems:
    scalar_key = ('scalar', system)
    avx2_key = ('avx2', system)
    avx512_key = ('avx512', system)
    
    if scalar_key in results and avx2_key in results:
        scalar_mean = statistics.mean(results[scalar_key])
        avx2_mean = statistics.mean(results[avx2_key])
        avx2_speedup = scalar_mean / avx2_mean
    else:
        avx2_speedup = 0.0
    
    if scalar_key in results and avx512_key in results:
        scalar_mean = statistics.mean(results[scalar_key])
        avx512_mean = statistics.mean(results[avx512_key])
        avx512_speedup = scalar_mean / avx512_mean
    else:
        avx512_speedup = 0.0
    
    print(f"{system:<10} {avx2_speedup:<15.3f} {avx512_speedup:<18.3f}")

PYEOF

echo ""
echo "Done!"

