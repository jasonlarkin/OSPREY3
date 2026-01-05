#!/usr/bin/env bash
set -euo pipefail

# Measure how coverage accumulates as labeled test categories are applied.
#
# This script:
#   - resets gcov counters once
#   - (optionally) captures a baseline tracefile before running any tests
#   - runs ctest for each label/category you specify (in order)
#   - captures an lcov tracefile after each step (cumulative coverage)
#   - writes a JSON manifest consumable by plot_kstar_coverage_accumulation.py
#
# Requirements:
#   - a GCC/Clang gcov-instrumented build dir with: -DKSTAR_ENABLE_COVERAGE=ON
#   - lcov installed
#
# Usage:
#   ./scripts/kstar_coverage_accumulation.sh [--build-dir DIR] [--mixture prod|all|tests] \
#       [--out-dir DIR] [--no-baseline] \
#       --steps LABEL_REGEX [LABEL_REGEX ...]
#
# Examples:
#   ./scripts/kstar_coverage_accumulation.sh --steps synthesized verbatim fuzz_corpus coverage_regression
#   ./scripts/kstar_coverage_accumulation.sh --mixture all --steps "synthesized" "verbatim"
#
# Outputs:
#   - tracefiles: <build_dir>/<out_dir>/coverage.<mixture>.stepNN.<label>.info
#   - manifest:   <build_dir>/<out_dir>/coverage_steps.<mixture>.json

BUILD_DIR="build/cpp/kstar-coverage"
OUT_DIR_REL="coverage-steps"
MIXTURE="prod"          # prod|all|tests
CAPTURE_BASELINE=1
STEPS=()

usage() {
  echo "usage: $0 [--build-dir DIR] [--mixture prod|all|tests] [--out-dir DIR] [--no-baseline] --steps LABEL [LABEL ...]" >&2
  exit 2
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --build-dir)
      BUILD_DIR="$2"; shift 2;;
    --out-dir)
      OUT_DIR_REL="$2"; shift 2;;
    --mixture)
      MIXTURE="$2"; shift 2;;
    --no-baseline)
      CAPTURE_BASELINE=0; shift 1;;
    --steps)
      shift
      while [[ $# -gt 0 ]] && [[ ! "$1" =~ ^-- ]]; do
        STEPS+=("$1")
        shift
      done
      ;;
    -h|--help)
      usage;;
    *)
      echo "unknown arg: $1" >&2
      usage;;
  esac
done

if [[ ${#STEPS[@]} -eq 0 ]]; then
  echo "ERROR: missing --steps" >&2
  usage
fi

case "${MIXTURE}" in
  prod|all|tests) ;;
  *)
    echo "ERROR: --mixture must be one of: prod|all|tests" >&2
    exit 2
    ;;
esac

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

if [[ ! -d "${REPO_ROOT}/${BUILD_DIR}" ]]; then
  echo "ERROR: build dir not found: ${REPO_ROOT}/${BUILD_DIR}" >&2
  echo "Hint: configure with:" >&2
  echo "  cmake -S src/main/cpp/kstar -B ${BUILD_DIR} -DBUILD_TESTING=ON -DKSTAR_ENABLE_COVERAGE=ON -DKSTAR_ENABLE_NATIVE_OPT=OFF" >&2
  exit 2
fi

cd "${REPO_ROOT}/${BUILD_DIR}"

LCOV_EXEC="${LCOV_EXEC:-lcov}"
if ! command -v "${LCOV_EXEC}" >/dev/null 2>&1; then
  echo "ERROR: lcov not found (LCOV_EXEC=${LCOV_EXEC}). Install with: sudo apt-get install -y lcov" >&2
  exit 2
fi

# Match kstar CMake coverage filters.
EXCLUDES_COMMON=(
  "/usr/*"
  "$(pwd)/_deps/*"
  "*googletest*"
  "*gtest*"
)
EXCLUDES_TESTS=(
  "*/src/test/*"
)
EXTRACT_TESTS=(
  "*/src/test/*"
)

OUT_DIR="$(pwd)/${OUT_DIR_REL}"
mkdir -p "${OUT_DIR}"

echo "[kstar_coverage_accumulation] repo=${REPO_ROOT}"
echo "[kstar_coverage_accumulation] build_dir=$(pwd)"
echo "[kstar_coverage_accumulation] out_dir=${OUT_DIR}"
echo "[kstar_coverage_accumulation] mixture=${MIXTURE}"
echo "[kstar_coverage_accumulation] steps=${STEPS[*]}"

echo "[kstar_coverage_accumulation] resetting coverage counters (deleting stale .gcda + zerocounters)"
find . -name "*.gcda" -delete || true
"${LCOV_EXEC}" --directory . --zerocounters >/dev/null

capture_one() {
  local step_idx="$1"   # 0-based
  local label_slug="$2" # safe-ish file token
  local raw="${OUT_DIR}/coverage.raw.step${step_idx}.${label_slug}.info"
  local all="${OUT_DIR}/coverage.all.step${step_idx}.${label_slug}.info"
  local out="${OUT_DIR}/coverage.${MIXTURE}.step${step_idx}.${label_slug}.info"

  # If no .gcda exists yet (eg baseline step before any tests run), use --initial to
  # capture a valid "0 hits" tracefile from .gcno.
  if ! find . -name "*.gcda" -print -quit | grep -q .; then
    "${LCOV_EXEC}" --capture --initial --directory . --output-file "${raw}" \
      --base-directory "${REPO_ROOT}" \
      --rc lcov_branch_coverage=1 >/dev/null
  else
    "${LCOV_EXEC}" --capture --directory . --output-file "${raw}" \
      --base-directory "${REPO_ROOT}" \
      --rc lcov_branch_coverage=1 >/dev/null
  fi

  "${LCOV_EXEC}" --remove "${raw}" "${EXCLUDES_COMMON[@]}" \
    --output-file "${all}" \
    --rc lcov_branch_coverage=1 >/dev/null

  case "${MIXTURE}" in
    all)
      cp "${all}" "${out}"
      ;;
    prod)
      "${LCOV_EXEC}" --remove "${all}" "${EXCLUDES_TESTS[@]}" \
        --output-file "${out}" \
        --rc lcov_branch_coverage=1 >/dev/null
      ;;
    tests)
      "${LCOV_EXEC}" --extract "${all}" "${EXTRACT_TESTS[@]}" \
        --output-file "${out}" \
        --rc lcov_branch_coverage=1 >/dev/null
      ;;
  esac
}

json_escape() {
  # Minimal JSON string escape for our inputs (labels).
  # IMPORTANT: do NOT use a heredoc here (it would consume stdin and always emit "").
  python3 -c 'import json,sys; print(json.dumps(sys.argv[1]))' "$1"
}

label_to_slug() {
  echo "$1" | tr ' /:;|()[]{}<>\t' '_' | tr -cd 'A-Za-z0-9_.-_' | cut -c1-64
}

MANIFEST="${OUT_DIR}/coverage_steps.${MIXTURE}.json"
tmp_manifest="$(mktemp)"
trap 'rm -f "${tmp_manifest}"' EXIT

{
  echo "{"
  echo "  \"repo_root\": $(json_escape "${REPO_ROOT}"),"
  echo "  \"build_dir\": $(json_escape "$(pwd)"),"
  echo "  \"out_dir\": $(json_escape "${OUT_DIR}"),"
  echo "  \"mixture\": $(json_escape "${MIXTURE}"),"
  echo "  \"steps\": ["
} > "${tmp_manifest}"

step_idx=0
if [[ "${CAPTURE_BASELINE}" == "1" ]]; then
  echo "[kstar_coverage_accumulation] capture baseline (no tests)"
  capture_one "${step_idx}" "baseline"
  {
    echo "    {"
    echo "      \"index\": ${step_idx},"
    echo "      \"label\": \"(baseline)\","
    echo "      \"ctest_label_regex\": null,"
    echo "      \"info_path\": $(json_escape "${OUT_DIR}/coverage.${MIXTURE}.step${step_idx}.baseline.info")"
    echo "    }"
  } >> "${tmp_manifest}"
  step_idx=$((step_idx+1))
fi

for label in "${STEPS[@]}"; do
  slug="$(label_to_slug "${label}")"
  echo "[kstar_coverage_accumulation] step ${step_idx}: run ctest -L ${label}"
  ctest --output-on-failure -L "${label}"
  echo "[kstar_coverage_accumulation] step ${step_idx}: capture lcov (${MIXTURE})"
  capture_one "${step_idx}" "${slug}"

  {
    echo "    ,{"
    echo "      \"index\": ${step_idx},"
    echo "      \"label\": $(json_escape "${label}"),"
    echo "      \"ctest_label_regex\": $(json_escape "${label}"),"
    echo "      \"info_path\": $(json_escape "${OUT_DIR}/coverage.${MIXTURE}.step${step_idx}.${slug}.info")"
    echo "    }"
  } >> "${tmp_manifest}"
  step_idx=$((step_idx+1))
done

{
  echo "  ]"
  echo "}"
} >> "${tmp_manifest}"

# Fix the leading comma if baseline was disabled.
if [[ "${CAPTURE_BASELINE}" == "0" ]]; then
  # Remove the first "    ," occurrence.
  python3 - <<PY
import pathlib
p = pathlib.Path("$tmp_manifest")
s = p.read_text()
s = s.replace("\n    ,{", "\n    {", 1)
p.write_text(s)
PY
fi

mv "${tmp_manifest}" "${MANIFEST}"
echo "[kstar_coverage_accumulation] wrote manifest: ${MANIFEST}"
echo "[kstar_coverage_accumulation] next: python3 scripts/tools/plot_kstar_coverage_accumulation.py ${MANIFEST}"

