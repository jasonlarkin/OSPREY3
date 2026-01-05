#!/usr/bin/env bash
#
# Build + run K* expandInto roofline benchmarks for:
#   - synthetic EnergyMatrix (sweep)
#   - real .emat.bin test_data cases
# under:
#   - SIMD ON
#   - SIMD OFF
#
# Then generate a combined plot with four series.
#
# Outputs (defaults):
#   build/cpp/kstar-perf/expandinto_roofline_synth_simd_on.json
#   build/cpp/kstar-perf/expandinto_roofline_real_simd_on.json
#   build/cpp/kstar-perf-scalar/expandinto_roofline_synth_simd_off.json
#   build/cpp/kstar-perf-scalar/expandinto_roofline_real_simd_off.json
#   build/cpp/kstar/roofline_plots/...
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_ON="${BUILD_ON:-build/cpp/kstar-perf}"
BUILD_OFF="${BUILD_OFF:-build/cpp/kstar-perf-scalar}"
OUT_DIR="${OUT_DIR:-build/cpp/kstar/roofline_plots}"

MIN_TIME="${MIN_TIME:-200ms}"
REPS="${REPS:-3}"

SYNTH_FILTER="${SYNTH_FILTER:-^BM_AStarFast_ExpandInto_Roofline/.*}"
REAL_FILTER="${REAL_FILTER:-^BM_AStarFast_ExpandInto_Roofline_RealEmat/.*}"

common_args=(
  --benchmark_min_time="$MIN_TIME"
  --benchmark_repetitions="$REPS"
  --benchmark_report_aggregates_only=true
  --benchmark_format=json
)

echo "=== Configure/build (SIMD ON) ==="
cmake -S src/main/cpp/kstar -B "$BUILD_ON" -DKSTAR_ENABLE_BENCHMARKS=ON -DKSTAR_ENABLE_SIMD_INTRINSICS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_ON" -j --target kstar_expandinto_roofline_bench

echo "=== Run synthetic (SIMD ON) ==="
cd "$BUILD_ON"
./kstar_expandinto_roofline_bench \
  --benchmark_filter="$SYNTH_FILTER" \
  --benchmark_out=expandinto_roofline_synth_simd_on.json \
  "${common_args[@]}"

echo "=== Run real .emat (SIMD ON) ==="
./kstar_expandinto_roofline_bench \
  --benchmark_filter="$REAL_FILTER" \
  --benchmark_out=expandinto_roofline_real_simd_on.json \
  "${common_args[@]}"
cd "$REPO_ROOT"

echo "=== Configure/build (SIMD OFF) ==="
cmake -S src/main/cpp/kstar -B "$BUILD_OFF" -DKSTAR_ENABLE_BENCHMARKS=ON -DKSTAR_ENABLE_SIMD_INTRINSICS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_OFF" -j --target kstar_expandinto_roofline_bench

echo "=== Run synthetic (SIMD OFF) ==="
cd "$BUILD_OFF"
./kstar_expandinto_roofline_bench \
  --benchmark_filter="$SYNTH_FILTER" \
  --benchmark_out=expandinto_roofline_synth_simd_off.json \
  "${common_args[@]}"

echo "=== Run real .emat (SIMD OFF) ==="
./kstar_expandinto_roofline_bench \
  --benchmark_filter="$REAL_FILTER" \
  --benchmark_out=expandinto_roofline_real_simd_off.json \
  "${common_args[@]}"
cd "$REPO_ROOT"

echo "=== Plot combined (4-series) ==="
mkdir -p "$OUT_DIR"
python3 scripts/tools/plot_kstar_expandinto_roofline.py \
  --input synth_simd_on="$BUILD_ON/expandinto_roofline_synth_simd_on.json" \
  --input real_simd_on="$BUILD_ON/expandinto_roofline_real_simd_on.json" \
  --input synth_simd_off="$BUILD_OFF/expandinto_roofline_synth_simd_off.json" \
  --input real_simd_off="$BUILD_OFF/expandinto_roofline_real_simd_off.json" \
  --out-dir "$OUT_DIR"

echo "Wrote:"
echo "  $OUT_DIR/kstar_expandinto_roofline.png"
echo "  $OUT_DIR/kstar_expandinto_working_set.png"

