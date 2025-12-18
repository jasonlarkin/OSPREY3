#!/bin/bash
# Empirically measure arithmetic intensity using perf hardware counters.
#
# Important:
# - This measures FLOPs using fp_arith_inst_retired.* counters.
# - This estimates bytes using cache-line based events (64B lines).
#   Prefer LLC load/store misses when available; otherwise fall back to cache-misses.
#
# Usage: ./measure_arithmetic_intensity.sh [system_size]

set -euo pipefail

BENCH_DIR="src/main/cc/ConfEcalc"
# Default output in repo root; override by setting OUTPUT_CSV env var
OUTPUT_CSV="${OUTPUT_CSV:-arithmetic_intensity_measurements.csv}"
SYSTEM_SIZE=${1:-"medium"}  # small, medium, large, xlarge, xxlarge

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT" || exit 1

# Create tmp directory
mkdir -p "$REPO_ROOT/tmp"

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

echo "=== Measuring Arithmetic Intensity ==="
echo "System: $SYSTEM_SIZE"
echo "  Atoms: $atoms, Amber pairs: $amber_pairs, EEF1 pairs: $eef1_pairs"
echo "  Iterations: $iterations"
echo ""

# Initialize CSV (append-safe)
if [ ! -f "$OUTPUT_CSV" ]; then
    echo "version,system_size,atoms,amber_pairs,eef1_pairs,flops_total,bytes_read,bytes_written,bytes_source,arithmetic_intensity,measured_gflops" > "$OUTPUT_CSV"
fi

# Function to measure arithmetic intensity for a version
measure_version() {
    local version=$1
    local bench_bin="$REPO_ROOT/$BENCH_DIR/build/benchmark_${version}_only"
    
    if [ ! -f "$bench_bin" ]; then
        echo "  ERROR: Benchmark binary not found: $bench_bin"
        return 1
    fi
    
    echo "Measuring $version..."
    
    # Run perf stat using CSV output (-x,) for robust parsing.
    # perf writes stats to stderr; capture all output.
    # Include system size to avoid overwriting across the outer loop.
    local debug_file="$REPO_ROOT/tmp/perf_output_${version}_${SYSTEM_SIZE}.txt"
    local events="fp_arith_inst_retired.scalar_single:u,fp_arith_inst_retired.scalar_double:u,fp_arith_inst_retired.128b_packed_double:u,fp_arith_inst_retired.256b_packed_double:u,fp_arith_inst_retired.512b_packed_double:u,cache-misses:u,cache-references:u,LLC-load-misses:u,LLC-store-misses:u,cycles:u"
    perf stat -x, -e "$events" -- "$bench_bin" "$atoms" "$amber_pairs" "$eef1_pairs" "$iterations" 2>&1 | tee "$debug_file" >/dev/null || true
    if [ "${OSPREY_SIMD_DEBUG:-0}" = "1" ]; then
        echo "  DEBUG: Saved perf output to $debug_file" >&2
    fi

    # If perf isn't installed / supported (common on WSL without linux-tools), mark as missing.
    if grep -qiE "perf not found for kernel|No permission to enable|not supported" "$debug_file"; then
        local missing="-1"
        local bytes_source="perf-unavailable"
        # still record timing-only GFLOP/s as missing too, since FLOPs are unknown without counters
        echo "$version,$SYSTEM_SIZE,$atoms,$amber_pairs,$eef1_pairs,$missing,$missing,$missing,$bytes_source,$missing,$missing" >> "$OUTPUT_CSV"
        echo "  perf unavailable; wrote missing counters (-1) for $version/$SYSTEM_SIZE" >&2
        return 0
    fi
    
    # Parse perf CSV into numeric counters (missing -> -1).
    # Emits: scalar_single scalar_double packed_128 packed_256 packed_512 cache_misses cache_refs llc_load_miss llc_store_miss cycles
    read -r scalar_single scalar_double packed_128 packed_256 packed_512 cache_misses cache_refs llc_load_miss llc_store_miss cycles <<EOF
$(python3 "$REPO_ROOT/scripts/tools/perf_parse_stat_csv.py" --input "$debug_file" --missing minus1 \
  fp_arith_inst_retired.scalar_single \
  fp_arith_inst_retired.scalar_double \
  fp_arith_inst_retired.128b_packed_double \
  fp_arith_inst_retired.256b_packed_double \
  fp_arith_inst_retired.512b_packed_double \
  cache-misses \
  cache-references \
  LLC-load-misses \
  LLC-store-misses \
  cycles
)
EOF
    
    # Calculate total FLOPs:
    # - scalar_* are 1 flop per instruction
    # - 128b packed doubles are 2 flops per instruction
    # - 256b packed doubles are 4 flops per instruction
    # - 512b packed doubles are 8 flops per instruction
    local total_flops=$(python3 -c "
vals = [$scalar_single,$scalar_double,$packed_128,$packed_256,$packed_512]
if any(v < 0 for v in vals):
    print(-1)
else:
    total = vals[0] + vals[1] + (vals[2] * 2) + (vals[3] * 4) + (vals[4] * 8)
    print(int(total))
" 2>/dev/null)
    # Estimate bytes moved using cache-line granularity events.
    # Prefer LLC misses (closest to DRAM traffic) when supported; otherwise fall back to cache-misses.
    local cache_line_size=64
    local bytes_read=-1
    local bytes_written=-1
    local bytes_source="missing"

    if [ "${llc_load_miss:-1}" -ge 0 ] && [ "${llc_store_miss:-1}" -ge 0 ] && { [ "${llc_load_miss}" -gt 0 ] || [ "${llc_store_miss}" -gt 0 ]; }; then
        bytes_read=$((llc_load_miss * cache_line_size))
        bytes_written=$((llc_store_miss * cache_line_size))
        bytes_source="LLC-misses"
    elif [ "${cache_misses:-1}" -ge 0 ] && [ "${cache_misses:-0}" -gt 0 ]; then
        # perf's cache-misses is typically last-level cache misses (platform-dependent).
        bytes_read=$((cache_misses * cache_line_size))
        bytes_written=0
        bytes_source="cache-misses"
    fi
    
    # Calculate arithmetic intensity
    local arithmetic_intensity=$(python3 -c "
flops = $total_flops
br = $bytes_read
bw = $bytes_written
if flops < 0 or br < 0 or bw < 0:
    print('-1.0')
else:
    total = br + bw
    if total > 0:
        print(f'{flops / total:.6f}')
    else:
        print('0.0')
" 2>/dev/null)
    
    # Get timing to calculate GFLOP/s
    local bench_output=$("$bench_bin" "$atoms" "$amber_pairs" "$eef1_pairs" "$iterations" 2>&1)
    local time_us=$(echo "$bench_output" | grep -E "[0-9]+\.[0-9]+.*us/iter" | tail -1 | awk '{print $1}' || echo "0")
    local time_sec=$(python3 -c "print($time_us * 1e-6)" 2>/dev/null)
    local measured_gflops=$(python3 -c "
flops = $total_flops
time_sec = $time_sec
if flops < 0 or time_sec <= 0:
    print('-1.0')
else:
    print(f'{(flops / time_sec) / 1e9:.6f}')
" 2>/dev/null)
    
    # Write to CSV
    echo "$version,$SYSTEM_SIZE,$atoms,$amber_pairs,$eef1_pairs,$total_flops,$bytes_read,$bytes_written,$bytes_source,$arithmetic_intensity,$measured_gflops" >> "$OUTPUT_CSV"
    
    if [ "${OSPREY_SIMD_DEBUG:-0}" = "1" ]; then
        echo "  Total FLOPs: $total_flops" >&2
        echo "  Bytes read: $bytes_read" >&2
        echo "  Bytes written: $bytes_written" >&2
        echo "  bytes_source: $bytes_source" >&2
        echo "  Arithmetic Intensity: $arithmetic_intensity FLOPs/byte" >&2
        echo "  Measured GFLOP/s: $measured_gflops" >&2
        echo "" >&2
    fi
}

# Measure all versions
for version in scalar avx2 avx512; do
    measure_version "$version"
done

echo "=== Results ==="
echo "Results saved to: $OUTPUT_CSV"
echo ""
cat "$OUTPUT_CSV" | column -t -s','

