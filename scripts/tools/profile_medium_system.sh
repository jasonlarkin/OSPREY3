#!/bin/bash
# Profile medium system with perf to understand AVX-512 anomaly
# Usage: ./profile_medium_system.sh

set -e

BENCH_DIR="src/main/cc/ConfEcalc"
OUTPUT_DIR="perf_medium_system"
NUM_RUNS=3

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

ATOMS=500
AMBER_PAIRS=1000
EEF1_PAIRS=500
ITERATIONS=20000

mkdir -p "$OUTPUT_DIR"

echo "=== Profiling Medium System ==="
echo "System: $ATOMS atoms, $AMBER_PAIRS Amber pairs, $EEF1_PAIRS EEF1 pairs"
echo "Iterations: $ITERATIONS"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Profile a single version
profile_version() {
    local version=$1
    local bench_bin="$REPO_ROOT/$BENCH_DIR/build/benchmark_${version}_only"
    
    echo "Profiling $version..."
    
    if [ ! -f "$bench_bin" ]; then
        echo "  ERROR: Benchmark binary not found: $bench_bin"
        return 1
    fi
    
    # perf stat for hardware metrics
    echo "  Running perf stat..."
    perf stat -e cycles,instructions,cache-misses,cache-references,\
        L1-dcache-loads,L1-dcache-load-misses,\
        L1-dcache-stores,L1-dcache-store-misses,\
        LLC-loads,LLC-load-misses,\
        fp_arith_inst_retired.256b_packed_double,\
        fp_arith_inst_retired.512b_packed_double,\
        cpu-cycles,instructions,\
        branches,branch-misses \
        -r $NUM_RUNS \
        -o "$OUTPUT_DIR/perf_stat_${version}.txt" \
        -- "$bench_bin" "$ATOMS" "$AMBER_PAIRS" "$EEF1_PAIRS" "$ITERATIONS" > /dev/null 2>&1
    
    # perf record for call graph
    echo "  Running perf record..."
    perf record -g -F 1000 -o "$OUTPUT_DIR/perf_data_${version}.data" \
        -- "$bench_bin" "$ATOMS" "$AMBER_PAIRS" "$EEF1_PAIRS" "$ITERATIONS" > /dev/null 2>&1
    
    # Generate report
    echo "  Generating report..."
    perf report -i "$OUTPUT_DIR/perf_data_${version}.data" --stdio \
        > "$OUTPUT_DIR/perf_report_${version}.txt" 2>&1 || true
    
    # Generate annotated source if possible
    perf annotate -i "$OUTPUT_DIR/perf_data_${version}.data" \
        > "$OUTPUT_DIR/perf_annotate_${version}.txt" 2>&1 || true
    
    echo "  Done: $OUTPUT_DIR/perf_*_${version}.*"
    echo ""
}

# Profile all versions
for version in scalar avx2 avx512; do
    profile_version "$version"
done

echo "=== Profiling Complete ==="
echo "View results:"
echo "  cat $OUTPUT_DIR/perf_stat_scalar.txt"
echo "  cat $OUTPUT_DIR/perf_stat_avx2.txt"
echo "  cat $OUTPUT_DIR/perf_stat_avx512.txt"
echo ""
echo "View reports:"
echo "  cat $OUTPUT_DIR/perf_report_scalar.txt"
echo "  cat $OUTPUT_DIR/perf_report_avx2.txt"
echo "  cat $OUTPUT_DIR/perf_report_avx512.txt"

