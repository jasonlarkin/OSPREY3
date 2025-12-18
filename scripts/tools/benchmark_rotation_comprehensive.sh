#!/bin/bash
# Comprehensive benchmark for rotation and coordinate transformation operations
# Similar methodology to energy calculation benchmarks

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cd "$REPO_ROOT"

BENCH_DIR="${REPO_ROOT}/src/main/cc/ConfEcalc/build"
OUTPUT_CSV="${REPO_ROOT}/rotation_benchmark_results.csv"

echo "=== Rotation Operations Comprehensive Benchmark ===" >&2
echo "Building..." >&2

cd src/main/cc/ConfEcalc
cmake -B build -DENABLE_SIMD=ON > /dev/null 2>&1
cmake --build build > /dev/null 2>&1
cd ../../../../

echo "Running benchmarks..." >&2

# System sizes based on typical molecule sizes from workload analysis:
# Small: 50 atoms (small molecule/ligand)
# Medium: 150 atoms (medium-sized molecule)
# Large: 300 atoms (large molecule/protein fragment)
# XLarge: 500 atoms (very large fragment)
# XXLarge: 1000 atoms (entire small protein)

# Iterations chosen to get measurable times (fewer iterations than energy calc)
declare -A SYSTEM_SIZES=(
    ["small"]="50 5000"
    ["medium"]="150 3000"
    ["large"]="300 2000"
    ["xlarge"]="500 1000"
    ["xxlarge"]="1000 500"
)

echo "version,system_size,num_atoms,iterations,time_us_total,time_us_per_iter" > "$OUTPUT_CSV"

for system_size in small medium large xlarge xxlarge; do
    read num_atoms iterations <<< "${SYSTEM_SIZES[$system_size]}"
    
    echo "  System: $system_size ($num_atoms atoms, $iterations iterations)" >&2
    
    # Scalar benchmark
    SCALAR_OUTPUT=$("$BENCH_DIR/benchmark_rotation_operations" "$num_atoms" "$iterations" 2>&1)
    SCALAR_TIME=$(echo "$SCALAR_OUTPUT" | grep "Scalar time:" | awk '{print $3}' | sed 's/us//')
    
    # Convert to microseconds per iteration (approximate from total time)
    if [ -n "$SCALAR_TIME" ]; then
        TIME_PER_ITER=$(python3 -c "print($SCALAR_TIME / $iterations)" 2>/dev/null || echo "0")
        echo "scalar,$system_size,$num_atoms,$iterations,$SCALAR_TIME,$TIME_PER_ITER" >> "$OUTPUT_CSV"
    fi
    
    # SIMD benchmark (if available)
    SIMD_TIME=$(echo "$SCALAR_OUTPUT" | grep "SIMD time:" | awk '{print $3}' | sed 's/us//' || echo "")
    if [ -n "$SIMD_TIME" ]; then
        TIME_PER_ITER=$(python3 -c "print($SIMD_TIME / $iterations)" 2>/dev/null || echo "0")
        echo "simd_avx2,$system_size,$num_atoms,$iterations,$SIMD_TIME,$TIME_PER_ITER" >> "$OUTPUT_CSV"
    fi
done

echo "" >&2
echo "Results saved to: $OUTPUT_CSV" >&2
echo "=== Benchmark Complete ===" >&2

# Print summary
echo "" >&2
echo "=== Summary ===" >&2
python3 << PYEOF
import csv
import sys

try:
    with open('$OUTPUT_CSV', 'r') as f:
        reader = csv.DictReader(f)
        rows = list(reader)
        
    if not rows:
        print("No results found", file=sys.stderr)
        sys.exit(1)
    
    print(f"{'System':<10} {'Version':<12} {'Time (us)':<12} {'Time/Iter (us)':<15}")
    print("-" * 60)
    
    for row in rows:
        print(f"{row['system_size']:<10} {row['version']:<12} {float(row['time_us_total']):<12.2f} {float(row['time_us_per_iter']):<15.4f}")
        
except Exception as e:
    print(f"Error reading results: {e}", file=sys.stderr)
    sys.exit(1)
PYEOF

