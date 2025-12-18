#!/bin/bash
# Analyze and compare perf profiles
# Usage: ./analyze_perf_profiles.sh [perf_results_dir]

set -e

RESULTS_DIR=${1:-perf_results}
LATEST=$(ls -td $RESULTS_DIR/perf_*_*.data 2>/dev/null | head -1 | sed 's/.*perf_\(.*\)_\([0-9]*\)\.data/\1_\2/')

if [ -z "$LATEST" ]; then
    echo "No perf results found in $RESULTS_DIR"
    exit 1
fi

echo "=== Perf Profile Analysis ==="
echo "Using timestamp: $LATEST"
echo ""

# Function to extract top functions from a profile
extract_top_functions() {
    local profile=$1
    local label=$2
    
    echo "=== $label Top Functions ==="
    perf report -i "$profile" --stdio 2>/dev/null | \
        grep -E "^[ ]+[0-9]+\.[0-9]+%" | \
        head -20 | \
        awk '{printf "  %6s %6s %s\n", $1, $2, $NF}'
    echo ""
}

# Function to get function-specific breakdown
extract_function_breakdown() {
    local profile=$1
    local func_name=$2
    
    echo "=== Breakdown for $func_name ==="
    perf report -i "$profile" --stdio 2>/dev/null | \
        grep -A 30 "$func_name" | \
        grep -E "^[ ]+[0-9]+\.[0-9]+%" | \
        head -10
    echo ""
}

# Analyze each profile
SCALAR_DATA="$RESULTS_DIR/perf_scalar_${LATEST}.data"
AVX2_DATA="$RESULTS_DIR/perf_avx2_exact_${LATEST}.data"
AVX512_DATA="$RESULTS_DIR/perf_avx512_exact_${LATEST}.data"

if [ -f "$SCALAR_DATA" ]; then
    extract_top_functions "$SCALAR_DATA" "SCALAR"
    extract_function_breakdown "$SCALAR_DATA" "calc_scalar"
fi

if [ -f "$AVX2_DATA" ]; then
    extract_top_functions "$AVX2_DATA" "AVX2"
    extract_function_breakdown "$AVX2_DATA" "calc_avx2"
fi

if [ -f "$AVX512_DATA" ]; then
    extract_top_functions "$AVX512_DATA" "AVX-512"
    extract_function_breakdown "$AVX512_DATA" "calc_avx512"
fi

# Compare profiles
if [ -f "$SCALAR_DATA" ] && [ -f "$AVX2_DATA" ]; then
    echo "=== SCALAR vs AVX2 Comparison ==="
    perf diff "$SCALAR_DATA" "$AVX2_DATA" 2>/dev/null | \
        grep -E "^[ ]+[0-9]+\.[0-9]+%" | \
        head -15
    echo ""
fi

if [ -f "$AVX2_DATA" ] && [ -f "$AVX512_DATA" ]; then
    echo "=== AVX2 vs AVX-512 Comparison ==="
    perf diff "$AVX2_DATA" "$AVX512_DATA" 2>/dev/null | \
        grep -E "^[ ]+[0-9]+\.[0-9]+%" | \
        head -15
    echo ""
fi

# Check for SIMD instructions
echo "=== Checking for SIMD Instruction Usage ==="
if [ -f "$AVX2_DATA" ]; then
    echo "AVX2 instructions (256-bit):"
    perf report -i "$AVX2_DATA" --stdio 2>/dev/null | grep -i "ymm\|_mm256" | head -5 || echo "  (not found in symbols)"
fi

if [ -f "$AVX512_DATA" ]; then
    echo "AVX-512 instructions (512-bit):"
    perf report -i "$AVX512_DATA" --stdio 2>/dev/null | grep -i "zmm\|_mm512" | head -5 || echo "  (not found in symbols)"
fi
echo ""

# Memory access analysis
echo "=== Memory Access Analysis ==="
echo "To get detailed memory stats, run:"
echo "  perf stat -e cache-misses,cache-references,LLC-loads,LLC-load-misses \\"
echo "    -o perf_results/mem_stats_scalar.txt ./build/benchmark_simd_direct 5000"

