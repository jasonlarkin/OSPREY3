#!/usr/bin/env bash
set -euo pipefail

# Merge/minimize/dedupe the ConfSearchAStar fuzz corpus using libFuzzer's -merge=1.
#
# Usage:
#   ./scripts/kstar_fuzz_merge_conf_search_astar_corpus.sh [DEST_CORPUS_DIR]
#
# Defaults:
#   DEST_CORPUS_DIR=build/cpp/kstar-fuzz/fuzz-corpus/conf_search_astar

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FUZZ_BUILD_DIR="${REPO_ROOT}/build/cpp/kstar-fuzz"

DEST_CORPUS_DIR="${1:-${FUZZ_BUILD_DIR}/fuzz-corpus/conf_search_astar}"
TMP_NEW_DIR="${DEST_CORPUS_DIR}.new"

FUZZ_BIN="${FUZZ_BUILD_DIR}/fuzz_conf_search_astar"

if [[ ! -x "${FUZZ_BIN}" ]]; then
  echo "[merge] missing fuzzer binary: ${FUZZ_BIN}" >&2
  echo "[merge] build it with:" >&2
  echo "  cmake -S src/main/cpp/kstar -B build/cpp/kstar-fuzz \\" >&2
  echo "    -DCMAKE_CXX_COMPILER=clang++ -DKSTAR_ENABLE_FUZZING=ON -DKSTAR_ENABLE_NATIVE_OPT=OFF -DBUILD_TESTING=OFF" >&2
  echo "  cmake --build build/cpp/kstar-fuzz -j --target fuzz_conf_search_astar" >&2
  exit 2
fi

mkdir -p "${DEST_CORPUS_DIR}"
rm -rf "${TMP_NEW_DIR}"
mkdir -p "${TMP_NEW_DIR}"

echo "[merge] src/dest=${DEST_CORPUS_DIR}"
echo "[merge] tmp=${TMP_NEW_DIR}"

"${FUZZ_BIN}" -merge=1 "${TMP_NEW_DIR}" "${DEST_CORPUS_DIR}"

rm -rf "${DEST_CORPUS_DIR}.bak"
mv "${DEST_CORPUS_DIR}" "${DEST_CORPUS_DIR}.bak"
mv "${TMP_NEW_DIR}" "${DEST_CORPUS_DIR}"

echo "[merge] done"
echo "[merge] new corpus: ${DEST_CORPUS_DIR}"
echo "[merge] old corpus backup: ${DEST_CORPUS_DIR}.bak"

