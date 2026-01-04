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
OUT_DIR="$(dirname "${OUT}")"
OUT_BASE="$(basename "${OUT}")"
OUT_STEM="${OUT_BASE%.tsv}"
OUT_NEW_LINES="${OUT_DIR}/${OUT_STEM}.new_lines.tsv"
OUT_NEW_BRANCHES="${OUT_DIR}/${OUT_STEM}.new_branches.tsv"
OUT_NEW_FUNCS="${OUT_DIR}/${OUT_STEM}.new_functions.tsv"
OUT_SUMMARY_MD="${OUT_DIR}/${OUT_STEM}.summary.md"

if [[ ! -f "${BASELINE}" ]]; then
  echo "missing baseline info: ${BASELINE}" >&2
  exit 2
fi
if [[ ! -f "${FUZZ}" ]]; then
  echo "missing fuzz info: ${FUZZ}" >&2
  exit 2
fi

mkdir -p "${OUT_DIR}"

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

# --- Hotspot lists: newly covered lines/branches/functions (baseline=0, fuzz>0) ---
tmp_lines_base="$(mktemp)"
tmp_lines_fuzz="$(mktemp)"
tmp_br_base="$(mktemp)"
tmp_br_fuzz="$(mktemp)"
tmp_fn_base="$(mktemp)"
tmp_fn_fuzz="$(mktemp)"
trap 'rm -f "$tmp_base" "$tmp_fuzz" "$tmp_lines_base" "$tmp_lines_fuzz" "$tmp_br_base" "$tmp_br_fuzz" "$tmp_fn_base" "$tmp_fn_fuzz"' EXIT

# Use an unlikely separator inside the join key (unit separator) to avoid ambiguity.
KEY_SEP=$'\x1f'

lines_detail_one() {
  # key<TAB>count where key = file<US>line
  awk -v sep="${KEY_SEP}" '
    /^SF:/ { sf = substr($0, 4); next }
    /^DA:/ {
      split(substr($0,4), a, ",")
      ln = a[1]
      cnt = a[2] + 0
      printf "%s%s%s\t%d\n", sf, sep, ln, cnt
      next
    }
  ' "$1"
}

branches_detail_one() {
  # key<TAB>taken where key = file<US>line<US>block<US>branch
  awk -v sep="${KEY_SEP}" '
    /^SF:/ { sf = substr($0, 4); next }
    /^BRDA:/ {
      split(substr($0,6), a, ",")
      ln = a[1]
      blk = a[2]
      br = a[3]
      taken = a[4]
      if (taken == "-" || taken == "") taken = 0
      printf "%s%s%s%s%s%s%s\t%d\n", sf, sep, ln, sep, blk, sep, br, taken + 0
      next
    }
  ' "$1"
}

funcs_detail_one() {
  # key<TAB>hits where key = file<US>func_name
  awk -v sep="${KEY_SEP}" '
    /^SF:/ { sf = substr($0, 4); next }
    /^FNDA:/ {
      split(substr($0,6), a, ",")
      hits = a[1] + 0
      # Function name may include commas in theory; join the rest back.
      fn = substr($0, 6 + length(a[1]) + 1)
      printf "%s%s%s\t%d\n", sf, sep, fn, hits
      next
    }
  ' "$1"
}

lines_detail_one "${BASELINE}" | sort -k1,1 > "${tmp_lines_base}"
lines_detail_one "${FUZZ}" | sort -k1,1 > "${tmp_lines_fuzz}"
branches_detail_one "${BASELINE}" | sort -k1,1 > "${tmp_br_base}"
branches_detail_one "${FUZZ}" | sort -k1,1 > "${tmp_br_fuzz}"
funcs_detail_one "${BASELINE}" | sort -k1,1 > "${tmp_fn_base}"
funcs_detail_one "${FUZZ}" | sort -k1,1 > "${tmp_fn_fuzz}"

{
  echo -e "file\tline\tfuzz_hits"
  join -t $'\t' -a1 -a2 -e 0 -o 0,1.2,2.2 "${tmp_lines_base}" "${tmp_lines_fuzz}" | \
  awk -F'\t' -v sep="${KEY_SEP}" '
    {
      key=$1; b=$2+0; f=$3+0
      if (b==0 && f>0) {
        gsub(sep, "\t", key)
        printf "%s\t%d\n", key, f
      }
    }
  ' | sort -t $'\t' -k3,3nr -k1,1
} > "${OUT_NEW_LINES}"

{
  echo -e "file\tline\tblock\tbranch\tfuzz_taken"
  join -t $'\t' -a1 -a2 -e 0 -o 0,1.2,2.2 "${tmp_br_base}" "${tmp_br_fuzz}" | \
  awk -F'\t' -v sep="${KEY_SEP}" '
    {
      key=$1; b=$2+0; f=$3+0
      if (b==0 && f>0) {
        gsub(sep, "\t", key)
        printf "%s\t%d\n", key, f
      }
    }
  ' | sort -t $'\t' -k5,5nr -k1,1
} > "${OUT_NEW_BRANCHES}"

{
  echo -e "file\tfunction\tfuzz_hits"
  join -t $'\t' -a1 -a2 -e 0 -o 0,1.2,2.2 "${tmp_fn_base}" "${tmp_fn_fuzz}" | \
  awk -F'\t' -v sep="${KEY_SEP}" '
    {
      key=$1; b=$2+0; f=$3+0
      if (b==0 && f>0) {
        gsub(sep, "\t", key)
        printf "%s\t%d\n", key, f
      }
    }
  ' | sort -t $'\t' -k3,3nr -k1,1
} > "${OUT_NEW_FUNCS}"

echo "[prod-delta] wrote: ${OUT_NEW_LINES}"
echo "[prod-delta] wrote: ${OUT_NEW_BRANCHES}"
echo "[prod-delta] wrote: ${OUT_NEW_FUNCS}"

# Human-readable summary (repo-relative paths, stable markdown)
relpath() {
  # strip repo root prefix if present (for readability)
  local p="$1"
  p="${p#${ROOT}/}"
  echo "${p}"
}

{
  echo "# Prod coverage delta (baseline vs fuzz)"
  echo ""
  echo "- Baseline info: \`$(relpath "${BASELINE}")\`"
  echo "- Fuzz info: \`$(relpath "${FUZZ}")\`"
  echo ""
  echo "## Top per-file deltas (by branches, then lines, then functions)"
  echo ""
  echo '```tsv'
  head -n 16 "${OUT}" | sed "s|${ROOT}/||g"
  echo '```'
  echo ""
  echo "## Top newly covered branches (baseline=0, fuzz>0)"
  echo ""
  echo '```tsv'
  head -n 21 "${OUT_NEW_BRANCHES}" | sed "s|${ROOT}/||g"
  echo '```'
  echo ""
  echo "## Top newly covered lines (baseline=0, fuzz>0)"
  echo ""
  echo '```tsv'
  head -n 21 "${OUT_NEW_LINES}" | sed "s|${ROOT}/||g"
  echo '```'
  echo ""
  echo "## Top newly covered functions (baseline=0, fuzz>0)"
  echo ""
  echo '```tsv'
  head -n 21 "${OUT_NEW_FUNCS}" | sed "s|${ROOT}/||g"
  echo '```'
} > "${OUT_SUMMARY_MD}"

echo "[prod-delta] wrote: ${OUT_SUMMARY_MD}"

echo ""
echo "[prod-delta] Top newly covered branches (first 20):"
head -n 21 "${OUT_NEW_BRANCHES}" 2>/dev/null || true
echo ""
echo "[prod-delta] Top newly covered lines (first 20):"
head -n 21 "${OUT_NEW_LINES}" 2>/dev/null || true
echo ""
echo "[prod-delta] Top newly covered functions (first 20):"
head -n 21 "${OUT_NEW_FUNCS}" 2>/dev/null || true


