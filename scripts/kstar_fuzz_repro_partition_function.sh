#!/usr/bin/env bash
set -euo pipefail

# Reproduce a libFuzzer artifact/crash for the PartitionFunction target with consistent sanitizer settings.
#
# Usage:
#   ./scripts/kstar_fuzz_repro_partition_function.sh <artifact_file> [fuzz_build_dir]
#
# Defaults:
#   fuzz_build_dir = build/cpp/kstar-fuzz

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT="${1:-}"
FUZZ_BUILD_DIR="${2:-${REPO_ROOT}/build/cpp/kstar-fuzz}"

if [[ -z "${ARTIFACT}" ]]; then
  echo "usage: $0 <artifact_file> [fuzz_build_dir]" >&2
  exit 2
fi
if [[ "${ARTIFACT}" == *"<"* || "${ARTIFACT}" == *">"* ]]; then
  echo "error: artifact path contains '<' or '>' (placeholder syntax)." >&2
  echo "use a real filename, e.g. build/cpp/kstar-fuzz/fuzz-artifacts/partition_function/crash-123" >&2
  exit 2
fi
if [[ ! -f "${ARTIFACT}" ]]; then
  echo "missing artifact file: ${ARTIFACT}" >&2
  exit 2
fi

FUZZ_BIN="${FUZZ_BUILD_DIR}/fuzz_partition_function"
if [[ ! -x "${FUZZ_BIN}" ]]; then
  echo "[repro] missing fuzzer binary: ${FUZZ_BIN}" >&2
  echo "[repro] building fuzzer (clang, libFuzzer)..." >&2
  cmake -S "${REPO_ROOT}/src/main/cpp/kstar" -B "${FUZZ_BUILD_DIR}" \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DKSTAR_ENABLE_FUZZING=ON \
    -DKSTAR_ENABLE_NATIVE_OPT=OFF \
    -DBUILD_TESTING=OFF
  cmake --build "${FUZZ_BUILD_DIR}" -j
fi

export ASAN_OPTIONS="${ASAN_OPTIONS:-symbolize=1:abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:fast_unwind_on_malloc=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1:abort_on_error=1}"

echo "[repro] fuzz_bin=${FUZZ_BIN}"
echo "[repro] artifact=${ARTIFACT}"
echo "[repro] ASAN_OPTIONS=${ASAN_OPTIONS}"
echo "[repro] UBSAN_OPTIONS=${UBSAN_OPTIONS}"

"${FUZZ_BIN}" -runs=1 "${ARTIFACT}"

