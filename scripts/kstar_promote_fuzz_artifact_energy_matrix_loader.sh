#!/usr/bin/env bash
set -euo pipefail

# Promote a fuzz artifact into a repo-local regression input for EnergyMatrixLoader,
# and ensure the regression gtest exists.
#
# Usage:
#   ./scripts/kstar_promote_fuzz_artifact_energy_matrix_loader.sh <artifact_file> [name]
#
# Output:
#   src/test_data/kstar/fuzz/energy_matrix_loader/<name>.bin

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ARTIFACT="${1:-}"
NAME="${2:-}"

if [[ -z "${ARTIFACT}" ]]; then
  echo "usage: $0 <artifact_file> [name]" >&2
  exit 2
fi
if [[ "${ARTIFACT}" == *"<"* || "${ARTIFACT}" == *">"* ]]; then
  echo "error: artifact path contains '<' or '>' (placeholder syntax)." >&2
  echo "use a real filename, e.g. build/cpp/kstar-fuzz/fuzz-corpus/energy_matrix_loader/12345" >&2
  exit 2
fi
if [[ ! -f "${ARTIFACT}" ]]; then
  echo "missing artifact file: ${ARTIFACT}" >&2
  exit 2
fi

if [[ -z "${NAME}" ]]; then
  base="$(basename "${ARTIFACT}")"
  # sanitize extension / weird artifact naming
  NAME="${base//[^A-Za-z0-9_.-]/_}"
fi

OUT_DIR="${REPO_ROOT}/src/test_data/kstar/fuzz/energy_matrix_loader"
mkdir -p "${OUT_DIR}"

OUT_FILE="${OUT_DIR}/${NAME}.bin"
MIN_FILE="${OUT_DIR}/${NAME}.min.bin"

echo "[promote] artifact=${ARTIFACT}"
echo "[promote] out=${OUT_FILE}"

# Best-effort minimize; if minimization fails, still promote original bytes.
if "${REPO_ROOT}/scripts/kstar_fuzz_minimize_energy_matrix_loader.sh" "${ARTIFACT}" "${MIN_FILE}" 2>/dev/null; then
  cp -f "${MIN_FILE}" "${OUT_FILE}"
  echo "[promote] minimized input stored"
else
  cp -f "${ARTIFACT}" "${OUT_FILE}"
  echo "[promote] minimization failed; stored original artifact bytes"
fi

echo "[promote] now add/commit:"
echo "  ${OUT_FILE}"
echo "[promote] run regression test via:"
echo "  ctest --test-dir build/cpp/kstar-coverage -R kstar.energy_matrix_loader_regression -V"


