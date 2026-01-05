#!/usr/bin/env bash
#
# Build + run K* expandInto roofline benchmark for SIMD ON/OFF and generate combined plots.
#
# Outputs:
#   build/cpp/kstar-perf/expandinto_roofline_simd_on.json
#   build/cpp/kstar-perf-scalar/expandinto_roofline_simd_off.json
#   build/cpp/kstar/roofline_plots/kstar_expandinto_roofline.png
#   build/cpp/kstar/roofline_plots/kstar_expandinto_working_set.png
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

BUILD_ON="${BUILD_ON:-build/cpp/kstar-perf}"
BUILD_OFF="${BUILD_OFF:-build/cpp/kstar-perf-scalar}"
OUT_DIR="${OUT_DIR:-build/cpp/kstar/roofline_plots}"

MIN_TIME="${MIN_TIME:-200ms}"
REPS="${REPS:-3}"

echo "=== Configure/build (SIMD ON) ==="
cmake -S src/main/cpp/kstar -B "$BUILD_ON" -DKSTAR_ENABLE_BENCHMARKS=ON -DKSTAR_ENABLE_SIMD_INTRINSICS=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_ON" -j --target kstar_expandinto_roofline_bench

echo "=== Run benchmark (SIMD ON) ==="
cd "$BUILD_ON"
./kstar_expandinto_roofline_bench \
  --benchmark_format=json \
  --benchmark_out=expandinto_roofline_simd_on.json \
  --benchmark_min_time="$MIN_TIME" \
  --benchmark_repetitions="$REPS" \
  --benchmark_report_aggregates_only=true
cd "$REPO_ROOT"

echo "=== Configure/build (SIMD OFF) ==="
cmake -S src/main/cpp/kstar -B "$BUILD_OFF" -DKSTAR_ENABLE_BENCHMARKS=ON -DKSTAR_ENABLE_SIMD_INTRINSICS=OFF -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build "$BUILD_OFF" -j --target kstar_expandinto_roofline_bench

echo "=== Run benchmark (SIMD OFF) ==="
cd "$BUILD_OFF"
./kstar_expandinto_roofline_bench \
  --benchmark_format=json \
  --benchmark_out=expandinto_roofline_simd_off.json \
  --benchmark_min_time="$MIN_TIME" \
  --benchmark_repetitions="$REPS" \
  --benchmark_report_aggregates_only=true
cd "$REPO_ROOT"

echo "=== Plot combined ==="
mkdir -p "$OUT_DIR"
python3 scripts/tools/plot_kstar_expandinto_roofline.py \
  --input simd_on="$BUILD_ON/expandinto_roofline_simd_on.json" \
  --input simd_off="$BUILD_OFF/expandinto_roofline_simd_off.json" \
  --out-dir "$OUT_DIR"

echo "Wrote:"
echo "  $OUT_DIR/kstar_expandinto_roofline.png"
echo "  $OUT_DIR/kstar_expandinto_working_set.png"

