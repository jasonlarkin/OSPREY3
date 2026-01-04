#!/usr/bin/env bash
set -euo pipefail

# Run libFuzzer briefly to grow corpus, then run GCC+lcov prod-only coverage.
#
# Usage:
#   ./scripts/kstar_fuzz_then_coverage.sh [FUZZ_SECONDS] [emat|astar|both]
#
# Outputs:
#   - fuzz corpus:    build/cpp/kstar-fuzz/fuzz-corpus/<harness>/
#   - fuzz artifacts: build/cpp/kstar-fuzz/fuzz-artifacts/<harness>/
#   - prod coverage:  build/cpp/kstar-coverage/coverage/kstar-prod/index.html
#
# Optional harness selection:
#   ./scripts/kstar_fuzz_then_coverage.sh [FUZZ_SECONDS] [emat|astar|both]

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
  both)
    run_fuzzer "energy_matrix_loader" "${FUZZ_BUILD_DIR}/fuzz_energy_matrix_loader" 65536
    run_fuzzer "conf_search_astar" "${FUZZ_BUILD_DIR}/fuzz_conf_search_astar" 2048
    ;;
  *)
    echo "usage: $0 [FUZZ_SECONDS] [emat|astar|both]" >&2
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
cmake --build "${COV_BUILD_DIR}" --target kstar_coverage_prod

echo "[kstar_fuzz_then_coverage] prod report: ${COV_BUILD_DIR}/coverage/kstar-prod/index.html"

