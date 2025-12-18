#!/bin/bash
# Analyze cache hierarchy usage across system sizes for the energy pair-computation benchmarks.
# Usage: ./analyze_cache_hierarchy.sh [system_size]
#
# Notes:
# - Uses perf CSV output (-x,) for stable parsing.
# - Unsupported counters are written as -1 so plots can omit them.

set -euo pipefail

BENCH_DIR="src/main/cc/ConfEcalc"
SYSTEM_SIZE=${1:-"medium"}  # small, medium, large, xlarge, xxlarge

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

mkdir -p "$REPO_ROOT/tmp"
mkdir -p "$REPO_ROOT/plots"

# System configurations
declare -A CONFIGS
CONFIGS[small]="200:2000:1000:50000"
CONFIGS[medium]="500:1000:500:20000"
CONFIGS[large]="1000:7500:500:10000"
CONFIGS[xlarge]="2000:15000:2000:5000"
CONFIGS[xxlarge]="5000:50000:5000:1000"

if [ -z "${CONFIGS[$SYSTEM_SIZE]}" ]; then
    echo "Error: Unknown system size: $SYSTEM_SIZE"
    echo "Available: small, medium, large, xlarge, xxlarge"
    exit 1
fi

IFS=':' read -r atoms amber_pairs eef1_pairs iterations <<< "${CONFIGS[$SYSTEM_SIZE]}"

echo "=== Cache Hierarchy Analysis ==="
echo "System: $SYSTEM_SIZE"
echo "  Atoms: $atoms, Amber pairs: $amber_pairs, EEF1 pairs: $eef1_pairs"
echo "  Iterations: $iterations"
echo ""

OUTPUT_CSV="$REPO_ROOT/tmp/cache_hierarchy_${SYSTEM_SIZE}.csv"
echo "version,L1_loads,L1_load_misses,L1_miss_rate,L1_stores,L1_store_misses,LLC_loads,LLC_load_misses,LLC_miss_rate,cache_misses,cache_references,cache_miss_rate" > "$OUTPUT_CSV"

# Function to analyze cache hierarchy for a version
analyze_version() {
    local version=$1
    local bench_bin="$REPO_ROOT/$BENCH_DIR/build/benchmark_${version}_only"
    
    if [ ! -f "$bench_bin" ]; then
        echo "  ERROR: Benchmark binary not found: $bench_bin"
        return 1
    fi
    
    echo "Analyzing $version cache hierarchy..."
    
    # Run perf stat with cache counters using CSV output for robust parsing.
    local debug_file="$REPO_ROOT/tmp/cache_perf_${version}_${SYSTEM_SIZE}.txt"
    local events="L1-dcache-loads:u,L1-dcache-load-misses:u,L1-dcache-stores:u,L1-dcache-store-misses:u,LLC-loads:u,LLC-load-misses:u,cache-misses:u,cache-references:u"
    perf stat -x, -e "$events" -- "$bench_bin" "$atoms" "$amber_pairs" "$eef1_pairs" "$iterations" 2>&1 | tee "$debug_file" >/dev/null || true

    # Parse counters. Unsupported counters become -1.
    read -r l1_loads l1_load_misses l1_stores l1_store_misses llc_loads llc_load_misses cache_misses cache_references <<EOF
$(python3 "$REPO_ROOT/scripts/tools/perf_parse_stat_csv.py" --input "$debug_file" --missing minus1 \
  L1-dcache-loads \
  L1-dcache-load-misses \
  L1-dcache-stores \
  L1-dcache-store-misses \
  LLC-loads \
  LLC-load-misses \
  cache-misses \
  cache-references
)
EOF
    
    # Calculate miss rates
    local l1_miss_rate=$(python3 -c "
l1_loads = $l1_loads
l1_load_misses = $l1_load_misses
if l1_loads < 0 or l1_load_misses < 0:
    print('-1.0')
elif l1_loads > 0:
    print(f'{l1_load_misses / l1_loads * 100:.4f}')
else:
    print('0.0')
" 2>/dev/null)
    
    local llc_miss_rate=$(python3 -c "
llc_loads = $llc_loads
llc_load_misses = $llc_load_misses
if llc_loads < 0 or llc_load_misses < 0:
    print('-1.0')
elif llc_loads > 0:
    print(f'{llc_load_misses / llc_loads * 100:.4f}')
else:
    print('0.0')
" 2>/dev/null)
    
    local cache_miss_rate=$(python3 -c "
cache_references = $cache_references
cache_misses = $cache_misses
if cache_references < 0 or cache_misses < 0:
    print('-1.0')
elif cache_references > 0:
    print(f'{cache_misses / cache_references * 100:.4f}')
else:
    print('0.0')
" 2>/dev/null)
    
    # Write to CSV
    echo "$version,$l1_loads,$l1_load_misses,$l1_miss_rate,$l1_stores,$l1_store_misses,$llc_loads,$llc_load_misses,$llc_miss_rate,$cache_misses,$cache_references,$cache_miss_rate" >> "$OUTPUT_CSV"
    
    # Print summary
    echo "  L1 Loads: $l1_loads"
    echo "  L1 Load Misses: $l1_load_misses (${l1_miss_rate}%)"
    echo "  L1 Stores: $l1_stores"
    echo "  L1 Store Misses: $l1_store_misses"
    echo "  LLC Loads: $llc_loads"
    echo "  LLC Load Misses: $llc_load_misses (${llc_miss_rate}%)"
    echo "  Cache Misses: $cache_misses / $cache_references (${cache_miss_rate}%)"
    echo ""
}

# Analyze all versions
for version in scalar avx2 avx512; do
    analyze_version "$version"
done

echo "=== Results ==="
echo "Results saved to: $OUTPUT_CSV"
echo ""
cat "$OUTPUT_CSV" | column -t -s','

