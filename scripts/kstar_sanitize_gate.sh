#!/usr/bin/env bash
set -euo pipefail

# Local crash-safety gate for the C++ K* port using clang sanitizers.
#
# This script intentionally uses:
#   -fsanitize=undefined
# by default on WSL.
#
# ASan (`-fsanitize=address`) is opt-in because some WSL setups crash before printing an ASan report
# (often during startup or gtest discovery). Enable it explicitly with:
#   KSTAR_SANITIZE_ASAN=1 ./scripts/kstar_sanitize_gate.sh
#
# Reason: ASan has integrated leak detection controlled by ASAN_OPTIONS=detect_leaks=1/0.
# Adding a standalone LeakSanitizer runtime (-fsanitize=leak) alongside ASan can cause
# sanitizer runtime conflicts and even segfaults during gtest discovery/listing.
#
# Usage:
#   ./scripts/kstar_sanitize_gate.sh
#
# Outputs:
#   build/cpp/kstar-sanitize/  (clang ASan/UBSan build dir)

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

# Put sanitizer build outputs on the Linux filesystem (not /mnt/c) to avoid WSL/DRVFS edge cases
# where ASan-instrumented binaries can crash during startup or gtest discovery.
#
# Source dir can remain under /mnt/c; only the build + executables need to live under $HOME.
if [[ "${KSTAR_SANITIZE_ASAN:-0}" == "1" ]]; then
  SAN_FLAGS="-fsanitize=address,undefined"
  BUILD_DIR="${HOME}/kstar-sanitize-asan"
else
  SAN_FLAGS="-fsanitize=undefined"
  BUILD_DIR="${HOME}/kstar-sanitize-ubsan"
fi

cmake -S "${REPO_ROOT}/src/main/cpp/kstar" -B "${BUILD_DIR}" \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DBUILD_TESTING=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=OFF \
  -DCMAKE_CXX_FLAGS="-O1 -g -fno-omit-frame-pointer ${SAN_FLAGS}" \
  -DCMAKE_EXE_LINKER_FLAGS="${SAN_FLAGS}"

cmake --build "${BUILD_DIR}" -j

export ASAN_OPTIONS="${ASAN_OPTIONS:-halt_on_error=1:abort_on_error=1:detect_leaks=0:symbolize=1}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"

# Do NOT use `ctest` in sanitizer builds on WSL: gtest discovery executes test binaries during
# test enumeration and can hard-fail the entire run if any unrelated gtest binary crashes early.
# Run the local "gate" binaries directly instead.

export KSTAR_CORPUS_RUNNER_STRICT=1

# If a corpus runner crashes, rerun with per-file tracing and a small limit to identify the repro file.
run_corpus_runner() {
  local name="$1"
  local bin="$2"
  echo "[kstar_sanitize_gate] run: ${name}"
  if "${bin}"; then
    return 0
  fi
  echo "[kstar_sanitize_gate] FAIL: ${name} crashed; rerun with tracing" >&2
  KSTAR_CORPUS_RUNNER_TRACE=1 KSTAR_CORPUS_RUNNER_LIMIT=50 "${bin}"
}

run_corpus_runner "energy_matrix_loader_corpus_runner" "${BUILD_DIR}/energy_matrix_loader_corpus_runner"
"${BUILD_DIR}/conf_search_astar_corpus_runner"
"${BUILD_DIR}/partition_function_corpus_runner"

"${BUILD_DIR}/energy_matrix_loader_regression_gtest"
"${BUILD_DIR}/partition_function_regression_gtest"
"${BUILD_DIR}/conf_search_astar_regression_gtest"

"${BUILD_DIR}/conf_search_astar_synthesized_gtest" --gtest_filter=ConfSearchAStar_SYNTHESIZED.*

