#!/usr/bin/env bash
set -euo pipefail

# Create a small, stable "what changed?" report for prod-only coverage baseline vs fuzz.
#
# Inputs are produced by:
#   cmake --build build/cpp/kstar-coverage --target kstar_coverage_prod_compare_fuzz
#
# Output:
#   build/cpp/kstar-coverage/coverage/prod_coverage_delta.tsv
#
# Usage:
#   ./scripts/kstar_coverage_prod_delta_report.sh [baseline_info] [fuzz_info] [out_tsv]

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

BASELINE="${1:-${ROOT}/build/cpp/kstar-coverage/coverage.prod.baseline.info}"
FUZZ="${2:-${ROOT}/build/cpp/kstar-coverage/coverage.prod.fuzz.info}"
OUT="${3:-${ROOT}/build/cpp/kstar-coverage/coverage/prod_coverage_delta.tsv}"

if [[ ! -f "${BASELINE}" ]]; then
  echo "missing baseline info: ${BASELINE}" >&2
  exit 2
fi
if [[ ! -f "${FUZZ}" ]]; then
  echo "missing fuzz info: ${FUZZ}" >&2
  exit 2
fi

mkdir -p "$(dirname "${OUT}")"

tmp_base="$(mktemp)"
tmp_fuzz="$(mktemp)"
trap 'rm -f "$tmp_base" "$tmp_fuzz"' EXIT

summarize_one() {
  # Outputs TSV:
  # file<TAB>lines_hit<TAB>lines_total<TAB>func_hit<TAB>func_total<TAB>br_hit<TAB>br_total
  awk '
    function flush() {
      if (sf == "") return
      printf "%s\t%d\t%d\t%d\t%d\t%d\t%d\n", sf, lh, lt, fh, ft, bh, bt
    }
    /^SF:/ {
      flush()
      sf = substr($0, 4)
      lh=lt=fh=ft=bh=bt=0
      next
    }
    /^DA:/ {
      lt++
      split(substr($0,4), a, ",")
      if (a[2] + 0 > 0) lh++
      next
    }
    /^FNDA:/ {
      ft++
      split(substr($0,6), a, ",")
      if (a[1] + 0 > 0) fh++
      next
    }
    /^BRDA:/ {
      bt++
      split(substr($0,6), a, ",")
      taken = a[4]
      if (taken != "-" && taken + 0 > 0) bh++
      next
    }
    END { flush() }
  ' "$1"
}

summarize_one "${BASELINE}" | sort -k1,1 > "${tmp_base}"
summarize_one "${FUZZ}" | sort -k1,1 > "${tmp_fuzz}"

{
  echo -e "file\td_lines_hit\td_funcs_hit\td_branches_hit\tbaseline(lines/funcs/branches)\tfuzz(lines/funcs/branches)"
  join -t $'\t' -a1 -a2 -e 0 -o \
    0,1.2,2.2,1.4,2.4,1.6,2.6,1.2,1.3,2.2,2.3,1.4,1.5,2.4,2.5,1.6,1.7,2.6,2.7 \
    "${tmp_base}" "${tmp_fuzz}" | \
  awk -F'\t' '
    {
      file=$1
      b_lh=$2; f_lh=$3
      b_fh=$4; f_fh=$5
      b_bh=$6; f_bh=$7
      d_lh = f_lh - b_lh
      d_fh = f_fh - b_fh
      d_bh = f_bh - b_bh

      b = sprintf("%d/%d %d/%d %d/%d", $8,$9,$12,$13,$16,$17)
      f = sprintf("%d/%d %d/%d %d/%d", $10,$11,$14,$15,$18,$19)

      printf "%s\t%d\t%d\t%d\t%s\t%s\n", file, d_lh, d_fh, d_bh, b, f
    }
  ' | sort -t $'\t' -k4,4nr -k2,2nr -k3,3nr
} > "${OUT}"

echo "[prod-delta] wrote: ${OUT}"


