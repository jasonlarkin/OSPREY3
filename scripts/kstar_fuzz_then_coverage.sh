#!/usr/bin/env bash
set -euo pipefail

# Run libFuzzer briefly to grow corpus, then run GCC+lcov prod-only coverage.
#
# Usage:
#   ./scripts/kstar_fuzz_then_coverage.sh [FUZZ_SECONDS] [emat|astar|pfunc|both|all]
#
# Outputs:
#   - fuzz corpus:    build/cpp/kstar-fuzz/fuzz-corpus/<harness>/
#   - fuzz artifacts: build/cpp/kstar-fuzz/fuzz-artifacts/<harness>/
#   - prod coverage:  build/cpp/kstar-coverage/coverage/kstar-prod-baseline/index.html
#                    build/cpp/kstar-coverage/coverage/kstar-prod-fuzz/index.html
#   - delta report:   build/cpp/kstar-coverage/coverage/prod_coverage_delta.summary.md
#
# Optional harness selection:
#   ./scripts/kstar_fuzz_then_coverage.sh [FUZZ_SECONDS] [emat|astar|pfunc|both|all]

FUZZ_SECONDS="${1:-30}"
HARNESS="${2:-emat}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

FUZZ_BUILD_DIR="${REPO_ROOT}/build/cpp/kstar-fuzz"
COV_BUILD_DIR="${REPO_ROOT}/build/cpp/kstar-coverage"

echo "[kstar_fuzz_then_coverage] repo=${REPO_ROOT}"
echo "[kstar_fuzz_then_coverage] fuzz_seconds=${FUZZ_SECONDS}"
echo "[kstar_fuzz_then_coverage] harness=${HARNESS}"

echo "[kstar_fuzz_then_coverage] configure fuzz build (clang, libFuzzer)"
cmake -S "${REPO_ROOT}/src/main/cpp/kstar" -B "${FUZZ_BUILD_DIR}" \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DKSTAR_ENABLE_FUZZING=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF \
  -DBUILD_TESTING=OFF

echo "[kstar_fuzz_then_coverage] build fuzz"
cmake --build "${FUZZ_BUILD_DIR}" -j

echo "[kstar_fuzz_then_coverage] seed fuzz corpora (cheap, deterministic)"
cmake --build "${FUZZ_BUILD_DIR}" -j --target kstar_fuzz_seed_energy_matrix_loader || true
cmake --build "${FUZZ_BUILD_DIR}" -j --target kstar_fuzz_seed_conf_search_astar || true

run_fuzzer() {
  local name="$1"
  local bin="$2"
  local max_len="$3"

  mkdir -p "${FUZZ_BUILD_DIR}/fuzz-corpus/${name}"
  mkdir -p "${FUZZ_BUILD_DIR}/fuzz-artifacts/${name}"

  echo "[kstar_fuzz_then_coverage] run fuzzer '${name}' (${FUZZ_SECONDS}s)"
  "${bin}" \
    -max_total_time="${FUZZ_SECONDS}" \
    -max_len="${max_len}" \
    -artifact_prefix="${FUZZ_BUILD_DIR}/fuzz-artifacts/${name}/" \
    "${FUZZ_BUILD_DIR}/fuzz-corpus/${name}"
}

case "${HARNESS}" in
  emat)
    run_fuzzer "energy_matrix_loader" "${FUZZ_BUILD_DIR}/fuzz_energy_matrix_loader" 65536
    ;;
  astar)
    run_fuzzer "conf_search_astar" "${FUZZ_BUILD_DIR}/fuzz_conf_search_astar" 2048
    ;;
  pfunc)
    run_fuzzer "partition_function" "${FUZZ_BUILD_DIR}/fuzz_partition_function" 2048
    ;;
  both)
    run_fuzzer "energy_matrix_loader" "${FUZZ_BUILD_DIR}/fuzz_energy_matrix_loader" 65536
    run_fuzzer "conf_search_astar" "${FUZZ_BUILD_DIR}/fuzz_conf_search_astar" 2048
    ;;
  all)
    run_fuzzer "energy_matrix_loader" "${FUZZ_BUILD_DIR}/fuzz_energy_matrix_loader" 65536
    run_fuzzer "conf_search_astar" "${FUZZ_BUILD_DIR}/fuzz_conf_search_astar" 2048
    run_fuzzer "partition_function" "${FUZZ_BUILD_DIR}/fuzz_partition_function" 2048
    ;;
  *)
    echo "usage: $0 [FUZZ_SECONDS] [emat|astar|pfunc|both|all]" >&2
    exit 2
    ;;
esac

echo "[kstar_fuzz_then_coverage] configure coverage build (gcc + lcov)"
cmake -S "${REPO_ROOT}/src/main/cpp/kstar" -B "${COV_BUILD_DIR}" \
  -DBUILD_TESTING=ON \
  -DKSTAR_ENABLE_COVERAGE=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF

echo "[kstar_fuzz_then_coverage] build + run prod-only coverage"
cmake --build "${COV_BUILD_DIR}" -j
cmake --build "${COV_BUILD_DIR}" --target kstar_coverage_prod_compare_fuzz

echo "[kstar_fuzz_then_coverage] prod report (baseline): ${COV_BUILD_DIR}/coverage/kstar-prod-baseline/index.html"
echo "[kstar_fuzz_then_coverage] prod report (with fuzz corpus replay): ${COV_BUILD_DIR}/coverage/kstar-prod-fuzz/index.html"
echo "[kstar_fuzz_then_coverage] prod delta summary: ${COV_BUILD_DIR}/coverage/prod_coverage_delta.summary.md"

