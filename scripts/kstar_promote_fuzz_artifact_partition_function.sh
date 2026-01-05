#!/usr/bin/env bash
set -euo pipefail

# Promote a fuzz artifact into a local regression/corpus input for PartitionFunction.
# This intentionally defaults to a build-local directory so we don't require committing
# binary inputs to the repo.
#
# Usage:
#   ./scripts/kstar_promote_fuzz_artifact_partition_function.sh <artifact_file> [name]
#
# Output (default):
#   build/cpp/kstar/test_data/fuzz/partition_function/<name>.bin
#
# Then replay via the corpus runner CTest:
#   KSTAR_FUZZ_CORPUS_DIR_PARTITION_FUNCTION=build/cpp/kstar/test_data/fuzz/partition_function \
#     ctest --test-dir build/cpp/kstar-coverage -R kstar.partition_function_corpus_runner -V

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT="${1:-}"
NAME="${2:-}"

if [[ -z "${ARTIFACT}" ]]; then
  echo "usage: $0 <artifact_file> [name]" >&2
  exit 2
fi
if [[ "${ARTIFACT}" == *"<"* || "${ARTIFACT}" == *">"* ]]; then
  echo "error: artifact path contains '<' or '>' (placeholder syntax)." >&2
  echo "use a real filename, e.g. build/cpp/kstar-fuzz/fuzz-corpus/partition_function/12345" >&2
  exit 2
fi
if [[ ! -f "${ARTIFACT}" ]]; then
  echo "missing artifact file: ${ARTIFACT}" >&2
  exit 2
fi

if [[ -z "${NAME}" ]]; then
  base="$(basename "${ARTIFACT}")"
  NAME="${base//[^A-Za-z0-9_.-]/_}"
fi

OUT_DIR="${REPO_ROOT}/build/cpp/kstar/test_data/fuzz/partition_function"
mkdir -p "${OUT_DIR}"

OUT_FILE="${OUT_DIR}/${NAME}.bin"
MIN_FILE="${OUT_DIR}/${NAME}.min.bin"

echo "[promote] artifact=${ARTIFACT}"
echo "[promote] out=${OUT_FILE}"

if "${REPO_ROOT}/scripts/kstar_fuzz_minimize_partition_function.sh" "${ARTIFACT}" "${MIN_FILE}" 2>/dev/null; then
  cp -f "${MIN_FILE}" "${OUT_FILE}"
  echo "[promote] minimized input stored"
else
  cp -f "${ARTIFACT}" "${OUT_FILE}"
  echo "[promote] minimization failed; stored original artifact bytes"
fi

echo "[promote] kept build-local (not committed):"
echo "  ${OUT_FILE}"
echo "[promote] replay via corpus runner CTest:"
echo "  KSTAR_FUZZ_CORPUS_DIR_PARTITION_FUNCTION=build/cpp/kstar/test_data/fuzz/partition_function \\"
echo "    ctest --test-dir build/cpp/kstar-coverage -R kstar.partition_function_corpus_runner -V"

