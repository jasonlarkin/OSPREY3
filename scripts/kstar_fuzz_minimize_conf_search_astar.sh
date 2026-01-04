#!/usr/bin/env bash
set -euo pipefail

# Minimize a crashing libFuzzer artifact for the ConfSearchAStar target.
#
# Usage:
#   ./scripts/kstar_fuzz_minimize_conf_search_astar.sh <artifact_file> <out_file> [fuzz_build_dir]
#
# Notes:
# - Requires a clang/libFuzzer build (KSTAR_ENABLE_FUZZING=ON).

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT="${1:-}"
OUT_FILE="${2:-}"
FUZZ_BUILD_DIR="${3:-${REPO_ROOT}/build/cpp/kstar-fuzz}"

if [[ -z "${ARTIFACT}" || -z "${OUT_FILE}" ]]; then
  echo "usage: $0 <artifact_file> <out_file> [fuzz_build_dir]" >&2
  exit 2
fi
if [[ "${ARTIFACT}" == *"<"* || "${ARTIFACT}" == *">"* ]]; then
  echo "error: artifact path contains '<' or '>' (placeholder syntax)." >&2
  echo "use a real filename, e.g. build/cpp/kstar-fuzz/fuzz-artifacts/conf_search_astar/crash-123" >&2
  exit 2
fi
if [[ ! -f "${ARTIFACT}" ]]; then
  echo "missing artifact file: ${ARTIFACT}" >&2
  exit 2
fi

FUZZ_BIN="${FUZZ_BUILD_DIR}/fuzz_conf_search_astar"
if [[ ! -x "${FUZZ_BIN}" ]]; then
  echo "missing fuzzer binary: ${FUZZ_BIN}" >&2
  echo "build it with: ./scripts/kstar_fuzz_then_coverage.sh (or configure build/cpp/kstar-fuzz)" >&2
  exit 2
fi

mkdir -p "$(dirname "${OUT_FILE}")"

export ASAN_OPTIONS="${ASAN_OPTIONS:-symbolize=1:abort_on_error=1:detect_leaks=0:allocator_may_return_null=1:fast_unwind_on_malloc=0}"
export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1:abort_on_error=1}"

echo "[minimize] fuzz_bin=${FUZZ_BIN}"
echo "[minimize] in=${ARTIFACT}"
echo "[minimize] out=${OUT_FILE}"

"${FUZZ_BIN}" \
  -minimize_crash=1 \
  -exact_artifact_path="${OUT_FILE}" \
  "${ARTIFACT}"

