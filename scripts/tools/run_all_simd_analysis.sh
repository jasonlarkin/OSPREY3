#!/bin/bash
# Re-run the full SIMD analysis pipeline end-to-end.
#
# Outputs (repo root):
# - benchmark_results.csv
# - arithmetic_intensity_measurements.csv
# - plots/*.png
# - tmp/* (cache hierarchy CSVs, perf raw outputs, rotation cache CSV)
#
# Usage:
#   ./scripts/tools/run_all_simd_analysis.sh [num_runs]
#
# Notes:
# - perf record/report may fail in some environments; this script continues.

set -euo pipefail

NUM_RUNS=${1:-5}
EXPERIMENT_LABEL=${2:-${OSPREY_EXPERIMENT_LABEL:-}}

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

echo "=== SIMD analysis pipeline ===" >&2
echo "num_runs: $NUM_RUNS" >&2
if [ -n "${EXPERIMENT_LABEL}" ]; then
  echo "experiment_label: ${EXPERIMENT_LABEL}" >&2
  export OSPREY_EXPERIMENT_LABEL="${EXPERIMENT_LABEL}"
fi
echo "" >&2

# Build everything needed
echo "Building ConfEcalc + benchmarks..." >&2
cd src/main/cc/ConfEcalc
cmake -B build -DENABLE_SIMD=ON >/dev/null
cmake --build build --target \
  ConfEcalc \
  benchmark_scalar_only benchmark_avx2_only benchmark_avx512_only \
  benchmark_simd_direct benchmark_simd_parameter_sweep \
  benchmark_rotation_operations benchmark_transrot \
  benchmark_compute_bound \
  >/dev/null
cd "$REPO_ROOT"

mkdir -p tmp plots
if [ -n "${EXPERIMENT_LABEL}" ]; then
  mkdir -p "plots/${EXPERIMENT_LABEL}"
fi

echo "" >&2
echo "Running comprehensive energy benchmarks..." >&2
bash scripts/tools/benchmark_comprehensive.sh "$NUM_RUNS" >/dev/null

echo "" >&2
echo "Generating benchmark plots..." >&2
python3 scripts/tools/plot_benchmark_results.py benchmark_results.csv >/dev/null
python3 scripts/tools/plot_benchmark_distributions.py benchmark_results.csv >/dev/null || true

echo "" >&2
echo "Measuring arithmetic intensity (all system sizes)..." >&2
rm -f arithmetic_intensity_measurements.csv
for s in small medium large xlarge xxlarge; do
  OUTPUT_CSV="arithmetic_intensity_measurements.csv" bash scripts/tools/measure_arithmetic_intensity.sh "$s" >/dev/null || true
done

echo "" >&2
echo "Generating roofline plots..." >&2
python3 scripts/tools/plot_roofline.py >/dev/null || true
python3 scripts/tools/plot_roofline_combined.py >/dev/null || true

echo "" >&2
echo "Cache hierarchy (energy pair computation)..." >&2
for s in small medium large xlarge xxlarge; do
  bash scripts/tools/analyze_cache_hierarchy.sh "$s" >/dev/null 2>&1 || true
  python3 scripts/tools/plot_cache_hierarchy.py "$s" >/dev/null 2>&1 || true
done

echo "" >&2
echo "Cache hierarchy (rotation/normalize kernels)..." >&2
bash scripts/tools/profile_rotation_cache_hierarchy.sh >/dev/null 2>&1 || true
python3 scripts/tools/plot_rotation_cache_hierarchy.py >/dev/null 2>&1 || true

echo "" >&2
echo "Perf profiling (optional; may fail depending on perf support)..." >&2
(
  cd src/main/cc/ConfEcalc
  bash ../../../../scripts/tools/profile_simd_stat.sh 1000 >/dev/null 2>&1 || true
  bash ../../../../scripts/tools/profile_simd.sh 5000 >/dev/null 2>&1 || true
  bash ../../../../scripts/tools/profile_simd_separate.sh 1000 >/dev/null 2>&1 || true
) || true

echo "" >&2
echo "Done." >&2


