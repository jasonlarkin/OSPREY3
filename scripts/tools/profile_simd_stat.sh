#!/bin/bash
# Profile SIMD with perf stat to get hardware-level metrics
# Usage: ./profile_simd_stat.sh [workload_size]

set -e

WORKLOAD=${1:-5000}
BENCHMARK_BIN="./build/benchmark_simd_direct"
OUTPUT_DIR="perf_results"

echo "=== SIMD Hardware-Level Profiling (perf stat) ==="
echo "Workload: $WORKLOAD iterations"
echo ""

mkdir -p "$OUTPUT_DIR"

# Function to profile with perf stat
profile_stat() {
    local version=$1
    local env_var=$2
    local output_file="$OUTPUT_DIR/perf_stat_${version}.txt"
    
    echo "Profiling $version with perf stat..."
    
    # Set environment variable if provided
    if [ -n "$env_var" ]; then
        export $env_var
    else
        unset OSPREY_FORCE_SCALAR
        unset OSPREY_USE_FAST_EXP
    fi
    
    # Run perf stat with key metrics
    perf stat -e \
        cycles,instructions,cache-misses,cache-references,\
        LLC-loads,LLC-load-misses,branch-misses,branches,\
        fp_arith_inst_retired.256b_packed_double,\
        fp_arith_inst_retired.512b_packed_double \
        -o "$output_file" \
        -- $BENCHMARK_BIN $WORKLOAD > /dev/null 2>&1
    
    echo "  Results saved to: $output_file"
    echo ""
    
    # Unset environment variable
    unset OSPREY_FORCE_SCALAR
    unset OSPREY_USE_FAST_EXP
}

# Profile all versions
echo "=== Profiling Scalar (baseline) ==="
profile_stat "scalar" "OSPREY_FORCE_SCALAR=1"

echo "=== Profiling AVX2 (exact exp) ==="
profile_stat "avx2_exact" ""

echo "=== Profiling AVX-512 (exact exp) ==="
profile_stat "avx512_exact" ""

# Generate comparison
echo "=== Generating Comparison ==="
cat > "$OUTPUT_DIR/perf_stat_comparison.txt" << 'EOF'
SIMD Hardware-Level Performance Comparison
==========================================

Key Metrics:
- cycles: Total CPU cycles
- instructions: Total instructions executed
- IPC (Instructions Per Cycle): instructions/cycles
- cache-misses: L1/L2 cache misses
- LLC-load-misses: Last-level cache (L3) misses
- fp_arith_inst_retired.256b_packed_double: AVX2 SIMD instructions
- fp_arith_inst_retired.512b_packed_double: AVX-512 SIMD instructions

EOF

echo "Comparison saved to: $OUTPUT_DIR/perf_stat_comparison.txt"
echo ""
echo "=== Profiling Complete ==="
echo "View results:"
echo "  cat $OUTPUT_DIR/perf_stat_scalar.txt"
echo "  cat $OUTPUT_DIR/perf_stat_avx2_exact.txt"
echo "  cat $OUTPUT_DIR/perf_stat_avx512_exact.txt"

