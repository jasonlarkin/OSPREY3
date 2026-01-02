#!/usr/bin/env bash
set -euo pipefail

# Run libFuzzer briefly to grow corpus, then run GCC+lcov prod-only coverage.
#
# Usage:
#   ./scripts/kstar_fuzz_then_coverage.sh [FUZZ_SECONDS]
#
# Outputs:
#   - fuzz corpus:    build/cpp/kstar-fuzz/fuzz-corpus/energy_matrix_loader/
#   - fuzz artifacts: build/cpp/kstar-fuzz/fuzz-artifacts/energy_matrix_loader/
#   - prod coverage:  build/cpp/kstar-coverage/coverage/kstar-prod/index.html

FUZZ_SECONDS="${1:-30}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

FUZZ_BUILD_DIR="${REPO_ROOT}/build/cpp/kstar-fuzz"
COV_BUILD_DIR="${REPO_ROOT}/build/cpp/kstar-coverage"

echo "[kstar_fuzz_then_coverage] repo=${REPO_ROOT}"
echo "[kstar_fuzz_then_coverage] fuzz_seconds=${FUZZ_SECONDS}"

echo "[kstar_fuzz_then_coverage] configure fuzz build (clang, libFuzzer)"
cmake -S "${REPO_ROOT}/src/main/cpp/kstar" -B "${FUZZ_BUILD_DIR}" \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DKSTAR_ENABLE_FUZZING=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF \
  -DBUILD_TESTING=OFF

echo "[kstar_fuzz_then_coverage] build fuzz"
cmake --build "${FUZZ_BUILD_DIR}" -j

echo "[kstar_fuzz_then_coverage] run fuzzer (${FUZZ_SECONDS}s)"
"${FUZZ_BUILD_DIR}/fuzz_energy_matrix_loader" \
  -max_total_time="${FUZZ_SECONDS}" \
  -max_len=65536 \
  -artifact_prefix="${FUZZ_BUILD_DIR}/fuzz-artifacts/energy_matrix_loader/" \
  "${FUZZ_BUILD_DIR}/fuzz-corpus/energy_matrix_loader"

echo "[kstar_fuzz_then_coverage] configure coverage build (gcc + lcov)"
cmake -S "${REPO_ROOT}/src/main/cpp/kstar" -B "${COV_BUILD_DIR}" \
  -DBUILD_TESTING=ON \
  -DKSTAR_ENABLE_COVERAGE=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF

echo "[kstar_fuzz_then_coverage] build + run prod-only coverage"
cmake --build "${COV_BUILD_DIR}" -j
cmake --build "${COV_BUILD_DIR}" --target kstar_coverage_prod

echo "[kstar_fuzz_then_coverage] prod report: ${COV_BUILD_DIR}/coverage/kstar-prod/index.html"

