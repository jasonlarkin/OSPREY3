#!/usr/bin/env bash
set -euo pipefail

# Merge/minimize/dedupe the PartitionFunction fuzz corpus using libFuzzer's -merge=1.
#
# This is useful after long fuzzing sessions to keep:
# - corpus-runner replay fast and stable
# - coverage delta runs deterministic-ish in runtime
#
# Usage:
#   ./scripts/kstar_fuzz_merge_partition_function_corpus.sh [DEST_CORPUS_DIR]
#
# Defaults:
#   DEST_CORPUS_DIR=build/cpp/kstar-fuzz/fuzz-corpus/partition_function
#
# Notes:
# - Requires a clang/libFuzzer fuzz build (KSTAR_ENABLE_FUZZING=ON) that produces `fuzz_partition_function`.
# - libFuzzer will rewrite DEST_CORPUS_DIR to a minimal set that preserves coverage.

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FUZZ_BUILD_DIR="${REPO_ROOT}/build/cpp/kstar-fuzz"

DEST_CORPUS_DIR="${1:-${FUZZ_BUILD_DIR}/fuzz-corpus/partition_function}"
TMP_NEW_DIR="${DEST_CORPUS_DIR}.new"

FUZZ_BIN="${FUZZ_BUILD_DIR}/fuzz_partition_function"

if [[ ! -x "${FUZZ_BIN}" ]]; then
  echo "[merge] missing fuzzer binary: ${FUZZ_BIN}" >&2
  echo "[merge] build it with:" >&2
  echo "  cmake -S src/main/cpp/kstar -B build/cpp/kstar-fuzz \\" >&2
  echo "    -DCMAKE_CXX_COMPILER=clang++ -DKSTAR_ENABLE_FUZZING=ON -DKSTAR_ENABLE_NATIVE_OPT=OFF -DBUILD_TESTING=OFF" >&2
  echo "  cmake --build build/cpp/kstar-fuzz -j --target fuzz_partition_function" >&2
  exit 2
fi

mkdir -p "${DEST_CORPUS_DIR}"
rm -rf "${TMP_NEW_DIR}"
mkdir -p "${TMP_NEW_DIR}"

echo "[merge] src/dest=${DEST_CORPUS_DIR}"
echo "[merge] tmp=${TMP_NEW_DIR}"

# libFuzzer merge mode expects:
#   fuzz_bin -merge=1 <DEST> <SRC1> <SRC2> ...
# We'll just merge DEST into TMP_NEW_DIR, then replace DEST.
"${FUZZ_BIN}" -merge=1 "${TMP_NEW_DIR}" "${DEST_CORPUS_DIR}"

rm -rf "${DEST_CORPUS_DIR}.bak"
mv "${DEST_CORPUS_DIR}" "${DEST_CORPUS_DIR}.bak"
mv "${TMP_NEW_DIR}" "${DEST_CORPUS_DIR}"

echo "[merge] done"
echo "[merge] new corpus: ${DEST_CORPUS_DIR}"
echo "[merge] old corpus backup: ${DEST_CORPUS_DIR}.bak"

