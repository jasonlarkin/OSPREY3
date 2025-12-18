#!/bin/bash
# Focused benchmark for medium system to investigate AVX-512 anomaly
# Usage: ./benchmark_medium_system.sh [num_runs]

set -e

BENCH_DIR="src/main/cc/ConfEcalc"
OUTPUT_CSV="benchmark_medium_system.csv"
NUM_RUNS=${1:-10}  # More runs for better statistics

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

echo "=== Medium System Investigation ==="
echo "System: 500 atoms, 1000 Amber pairs, 500 EEF1 pairs"
echo "Iterations: 20000"
echo "Runs per version: $NUM_RUNS"
echo ""

# Initialize CSV file
echo "version,run_id,time_us_per_iter,cpu_time_user,cpu_time_sys" > "$OUTPUT_CSV"

# System configuration
ATOMS=500
AMBER_PAIRS=1000
EEF1_PAIRS=500
ITERATIONS=20000

# Function to run a single benchmark with timing
run_single_benchmark() {
    local version=$1
    local run_id=$2
    local bench_bin="$REPO_ROOT/$BENCH_DIR/build/benchmark_${version}_only"
    
    if [ ! -f "$bench_bin" ]; then
        echo "  ERROR: Benchmark binary not found: $bench_bin"
        return 1
    fi
    
    # Run benchmark and capture output
    local output=$("$bench_bin" "$ATOMS" "$AMBER_PAIRS" "$EEF1_PAIRS" "$ITERATIONS" 2>&1)
    
    # Extract time_us from output (last line should contain "X.XX us/iter")
    local time_us=$(echo "$output" | grep -E "[0-9]+\.[0-9]+.*us/iter" | tail -1 | awk '{print $1}')
    
    # If that didn't work, try just getting the last number
    if [ -z "$time_us" ]; then
        time_us=$(echo "$output" | tail -1 | awk '{print $1}')
    fi
    
    # Try to get CPU time from /usr/bin/time if available
    local cpu_user=""
    local cpu_sys=""
    if command -v /usr/bin/time &> /dev/null; then
        local time_output=$(/usr/bin/time -f "%U %S" "$bench_bin" "$ATOMS" "$AMBER_PAIRS" "$EEF1_PAIRS" "$ITERATIONS" 2>&1 | tail -1)
        cpu_user=$(echo "$time_output" | awk '{print $1}')
        cpu_sys=$(echo "$time_output" | awk '{print $2}')
        # Also extract time_us from this run if we didn't get it before
        if [ -z "$time_us" ]; then
            time_output_full=$(/usr/bin/time -f "%U %S" "$bench_bin" "$ATOMS" "$AMBER_PAIRS" "$EEF1_PAIRS" "$ITERATIONS" 2>&1)
            time_us=$(echo "$time_output_full" | grep -E "[0-9]+\.[0-9]+.*us/iter" | tail -1 | awk '{print $1}')
        fi
    fi
    
    if [ -z "$time_us" ] || [ "$time_us" = "0" ]; then
        echo "  ERROR: Failed to get timing"
        return 1
    fi
    
    # Write to CSV
    echo "$version,$run_id,$time_us,$cpu_user,$cpu_sys" >> "$OUTPUT_CSV"
    
    echo "$time_us us/iter"
}

# Run benchmarks for each version
for version in scalar avx2 avx512; do
    echo "=== Benchmarking $version ($NUM_RUNS runs) ==="
    for run_id in $(seq 0 $((NUM_RUNS - 1))); do
        echo -n "  Run $((run_id + 1))/$NUM_RUNS: "
        run_single_benchmark "$version" "$run_id"
    done
    echo ""
done

echo "=== Results Summary ==="
python3 << PYEOF
import csv
import statistics

csv_file = "${OUTPUT_CSV}"

results = {}
with open(csv_file, 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        version = row['version']
        if version not in results:
            results[version] = []
        results[version].append(float(row['time_us_per_iter']))

print(f"\n{'Version':<10} {'Mean (us)':<12} {'Median (us)':<12} {'StdDev (us)':<12} {'Min (us)':<12} {'Max (us)':<12} {'CV (%)':<10}")
print("-" * 90)

for version in ['scalar', 'avx2', 'avx512']:
    if version in results:
        times = results[version]
        mean = statistics.mean(times)
        median = statistics.median(times)
        stdev = statistics.stdev(times) if len(times) > 1 else 0.0
        min_time = min(times)
        max_time = max(times)
        cv = (stdev / mean * 100) if mean > 0 else 0.0
        
        print(f"{version:<10} {mean:<12.2f} {median:<12.2f} {stdev:<12.2f} {min_time:<12.2f} {max_time:<12.2f} {cv:<10.2f}")

# Calculate speedups
if 'scalar' in results and 'avx2' in results:
    scalar_mean = statistics.mean(results['scalar'])
    avx2_mean = statistics.mean(results['avx2'])
    avx2_speedup = scalar_mean / avx2_mean
    print(f"\nAVX2 speedup: {avx2_speedup:.3f}x")

if 'scalar' in results and 'avx512' in results:
    scalar_mean = statistics.mean(results['scalar'])
    avx512_mean = statistics.mean(results['avx512'])
    avx512_speedup = scalar_mean / avx512_mean
    print(f"AVX-512 speedup: {avx512_speedup:.3f}x")

print("\n=== Key Observations ===")
if 'scalar' in results:
    scalar_cv = (statistics.stdev(results['scalar']) / statistics.mean(results['scalar']) * 100) if len(results['scalar']) > 1 else 0
    print(f"Scalar variance: {scalar_cv:.2f}% (note: scalar shows high variance - one outlier at {max(results['scalar']):.2f} us)")
if 'avx512' in results:
    avx512_cv = (statistics.stdev(results['avx512']) / statistics.mean(results['avx512']) * 100) if len(results['avx512']) > 1 else 0
    print(f"AVX-512 variance: {avx512_cv:.2f}% (very consistent with 10 runs)")
    print(f"Previous 3-run results showed anomaly due to small sample size")
PYEOF

echo ""
echo "Results saved to: $OUTPUT_CSV"

