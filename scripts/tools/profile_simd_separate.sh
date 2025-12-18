#!/bin/bash
# Profile each SIMD version separately for accurate results
# Usage: ./profile_simd_separate.sh [workload_size]

set -e

WORKLOAD=${1:-1000}
BENCH_DIR="src/main/cc/ConfEcalc"
OUTPUT_DIR="perf_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

cd "$(dirname "$0")/../../.." || exit 1

echo "=== SIMD Performance Profiling (Separate Runs) ==="
echo "Workload: $WORKLOAD iterations"
echo "Output directory: $OUTPUT_DIR"
echo ""

mkdir -p "$OUTPUT_DIR"

# Profile a single version
profile_version() {
    local version=$1
    local bench_bin="$BENCH_DIR/build/benchmark_${version}_only"
    local output_file="$OUTPUT_DIR/perf_stat_${version}_${TIMESTAMP}.txt"
    local report_file="$OUTPUT_DIR/perf_report_${version}_${TIMESTAMP}.txt"
    
    echo "Profiling $version..."
    
    if [ ! -f "$bench_bin" ]; then
        echo "  Benchmark binary not found: $bench_bin"
        echo "  Building..."
        cd "$BENCH_DIR" || exit 1
        cmake -B build -DENABLE_SIMD=ON > /dev/null 2>&1
        cmake --build build > /dev/null 2>&1
        cd - > /dev/null || exit 1
    fi
    
    if [ ! -f "$bench_bin" ]; then
        echo "  Failed to build $version benchmark"
        return 1
    fi
    
    # Run perf stat with detailed cache metrics
    perf stat -e cycles,instructions,cache-misses,cache-references,\
        L1-dcache-loads,L1-dcache-load-misses,\
        L1-dcache-stores,L1-dcache-store-misses,\
        L1-icache-loads,L1-icache-load-misses,\
        LLC-loads,LLC-load-misses,\
        fp_arith_inst_retired.256b_packed_double,\
        fp_arith_inst_retired.512b_packed_double,\
        cpu-cycles,instructions \
        -o "$output_file" \
        -- "$bench_bin" $WORKLOAD > /dev/null 2>&1
    
    # Run perf record for detailed analysis
    perf record -g -o "$OUTPUT_DIR/perf_${version}_${TIMESTAMP}.data" \
        -- "$bench_bin" $WORKLOAD > /dev/null 2>&1
    
    perf report -i "$OUTPUT_DIR/perf_${version}_${TIMESTAMP}.data" --stdio > "$report_file" 2>&1 || true
    
    echo "  Results: $output_file"
    echo "  Report: $report_file"
    echo ""
}

# Profile all versions separately
echo "=== Profiling Scalar ==="
profile_version "scalar"

echo "=== Profiling AVX2 ==="
profile_version "avx2"

echo "=== Profiling AVX-512 ==="
profile_version "avx512"

echo "=== Profiling Complete ==="
echo "View results:"
echo "  cat $OUTPUT_DIR/perf_stat_scalar_${TIMESTAMP}.txt"
echo "  cat $OUTPUT_DIR/perf_stat_avx2_${TIMESTAMP}.txt"
echo "  cat $OUTPUT_DIR/perf_stat_avx512_${TIMESTAMP}.txt"
