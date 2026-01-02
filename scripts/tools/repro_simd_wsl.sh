#!/usr/bin/env bash
# Reproducibility runner for ConfEcalc SIMD microbenchmarks under WSL.
#
# What it does:
# - Ensures `perf` wrapper works on WSL by wiring `/usr/lib/linux-tools/$(uname -r)/perf`
#   to an installed perf binary (often from linux-tools-<ubuntu-kernel>).
# - Runs deterministic, CPU-pinned repeats of:
#   - ConfEcalc "only" benchmarks: benchmark_scalar_only / benchmark_avx2_only / benchmark_avx512_only
#   - ConfEcalc direct benchmark: benchmark_simd_direct
# - Runs `perf stat` repeats (user-space counters, :u) and writes CSV output.
#
# Usage:
#   bash scripts/tools/repro_simd_wsl.sh
#   bash scripts/tools/repro_simd_wsl.sh /path/to/output_dir
#
# Notes:
# - This script is WSL-focused. On native Linux you usually don't need the perf wrapper fix.
# - If your WSL user can't `sudo -n`, run the perf wiring step manually once:
#     sudo mkdir -p "/usr/lib/linux-tools/$(uname -r)"
#     sudo ln -sf /usr/lib/linux-tools-*/perf "/usr/lib/linux-tools/$(uname -r)/perf"
#
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

OUTDIR="${1:-}"
if [ -z "$OUTDIR" ]; then
  TS="$(date +%Y%m%d_%H%M%S)"
  OUTDIR="$REPO_ROOT/perf_results/repro_${TS}"
fi
mkdir -p "$OUTDIR"

BENCH_DIR="$REPO_ROOT/src/main/cc/ConfEcalc"
BUILD_DIR="$BENCH_DIR/build"

# Defaults: match your earlier notes/benchmarks
ATOMS="${ATOMS:-200}"
AMBER="${AMBER:-2000}"
EEF1="${EEF1:-1000}"
BENCH_ITERS="${BENCH_ITERS:-20000}"     # for benchmark_*_only wallclock repeats
BENCH_REPS="${BENCH_REPS:-15}"
DIRECT_ITERS="${DIRECT_ITERS:-5000}"
DIRECT_REPS="${DIRECT_REPS:-5}"
PERF_ITERS="${PERF_ITERS:-2000}"       # for perf stat runs (keep shorter)
PERF_REPS="${PERF_REPS:-10}"           # perf's internal repeat count (-r)
PIN_CORE="${PIN_CORE:-0}"

PIN_CMD=(taskset -c "$PIN_CORE")

log() { echo "$@" | tee -a "$OUTDIR/run.log" >&2; }

log "OUTDIR: $OUTDIR"
log "params: ATOMS=$ATOMS AMBER=$AMBER EEF1=$EEF1"
log "bench:  BENCH_ITERS=$BENCH_ITERS BENCH_REPS=$BENCH_REPS"
log "direct: DIRECT_ITERS=$DIRECT_ITERS DIRECT_REPS=$DIRECT_REPS"
log "perf:   PERF_ITERS=$PERF_ITERS PERF_REPS=$PERF_REPS"
log "pin:    core=$PIN_CORE"

{
  echo "== timestamp =="; date -Is
  echo "== uname =="; uname -a
  echo "== lscpu =="; lscpu || true
  echo "== perf_event_paranoid =="; cat /proc/sys/kernel/perf_event_paranoid || true
  echo "== kptr_restrict =="; cat /proc/sys/kernel/kptr_restrict || true
} > "$OUTDIR/env.txt" 2>&1 || true

ensure_perf_wrapper() {
  if ! command -v perf >/dev/null 2>&1; then
    log "ERROR: perf not installed (install linux-tools-common + linux-tools-virtual/generic)."
    return 1
  fi

  if perf --version 2>&1 | grep -qi "perf not found for kernel"; then
    local krel
    krel="$(uname -r)"
    local real_perf
    real_perf="$(ls -1 /usr/lib/linux-tools-*/perf 2>/dev/null | head -n 1 || true)"
    if [ -z "$real_perf" ]; then
      log "ERROR: could not find an installed perf binary under /usr/lib/linux-tools-*/perf"
      return 1
    fi

    # Need non-interactive sudo inside automation.
    if ! sudo -n true 2>/dev/null; then
      log "ERROR: perf wrapper needs wiring, but sudo -n is unavailable."
      log "       Fix manually: sudo mkdir -p \"/usr/lib/linux-tools/$krel\" && sudo ln -sf \"$real_perf\" \"/usr/lib/linux-tools/$krel/perf\""
      return 1
    fi

    sudo -n mkdir -p "/usr/lib/linux-tools/$krel"
    sudo -n ln -sf "$real_perf" "/usr/lib/linux-tools/$krel/perf"
  fi

  perf --version > "$OUTDIR/perf_version.txt" 2>&1 || true

  # Materialize perf list (avoid head/pipefail SIGPIPE issues)
  perf list --no-desc > "$OUTDIR/perf_list.txt" 2>&1 || true
  head -n 120 "$OUTDIR/perf_list.txt" > "$OUTDIR/perf_list_head120.txt" 2>&1 || true
}

build_benchmarks() {
  log "Building ConfEcalc benchmarks..."
  cd "$BENCH_DIR"
  cmake -B build -DENABLE_SIMD=ON -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build build --target \
    ConfEcalc \
    benchmark_scalar_only benchmark_avx2_only benchmark_avx512_only \
    benchmark_simd_direct >/dev/null
  cd "$REPO_ROOT"

  ls -lh "$BUILD_DIR"/benchmark_* > "$OUTDIR/build_outputs.txt" 2>&1 || true
}

event_supported() {
  # Returns 0 if perf can measure the event in user-space mode.
  local ev="$1"
  local out
  out="$(perf stat -e "${ev}:u" -- true 2>&1 || true)"
  if echo "$out" | grep -Eqi "(not supported|unknown event|event syntax error|No such file or directory|failed to open)"; then
    return 1
  fi
  return 0
}

select_events() {
  local candidates=(cycles instructions cache-references cache-misses branch-instructions branch-misses)
  # Bash nounset + empty arrays can be finicky across environments; declare explicitly.
  local -a selected
  selected=()
  for ev in "${candidates[@]}"; do
    if event_supported "$ev"; then
      selected+=("${ev}:u")
    fi
  done
  # If no events are available, return empty string (caller will skip perf stat).
  (IFS=,; echo "${selected[*]-}")
}

run_repeats() {
  local name="$1"
  local exe="$2"
  log "Running repeats: $name"
  : > "$OUTDIR/${name}.txt"
  for r in $(seq 1 "$BENCH_REPS"); do
    "${PIN_CMD[@]}" "$exe" "$ATOMS" "$AMBER" "$EEF1" "$BENCH_ITERS" 2>&1 \
      | sed -e "s/^/[rep $r] /" | tee -a "$OUTDIR/${name}.txt" >/dev/null
  done
}

run_direct() {
  local exe="$1"
  log "Running direct benchmark (includes correctness prints)"
  : > "$OUTDIR/benchmark_simd_direct.txt"
  for r in $(seq 1 "$DIRECT_REPS"); do
    "${PIN_CMD[@]}" "$exe" "$DIRECT_ITERS" 2>&1 \
      | sed -e "s/^/[rep $r] /" | tee -a "$OUTDIR/benchmark_simd_direct.txt" >/dev/null
  done
}

run_perf_stat() {
  local name="$1"
  local exe="$2"
  local events="$3"

  if [ -z "$events" ]; then
    log "Skipping perf stat ($name): no supported events detected"
    return 0
  fi

  log "perf stat ($name): events=$events"
  # Keep stdout/stderr separate and machine-parsable (-x,)
  "${PIN_CMD[@]}" perf stat -r "$PERF_REPS" -x, -e "$events" -- \
    "$exe" "$ATOMS" "$AMBER" "$EEF1" "$PERF_ITERS" \
    1> "$OUTDIR/perf_${name}.out" \
    2> "$OUTDIR/perf_${name}.csv" || true
}

ensure_perf_wrapper
build_benchmarks

SCALAR_EXE="$BUILD_DIR/benchmark_scalar_only"
AVX2_EXE="$BUILD_DIR/benchmark_avx2_only"
AVX512_EXE="$BUILD_DIR/benchmark_avx512_only"
DIRECT_EXE="$BUILD_DIR/benchmark_simd_direct"

run_repeats "benchmark_scalar_only" "$SCALAR_EXE"
run_repeats "benchmark_avx2_only" "$AVX2_EXE"
run_repeats "benchmark_avx512_only" "$AVX512_EXE"
run_direct "$DIRECT_EXE"

EVENTS="$(select_events)"
echo "$EVENTS" > "$OUTDIR/perf_events_selected.txt"

run_perf_stat "scalar_only" "$SCALAR_EXE" "$EVENTS"
run_perf_stat "avx2_only" "$AVX2_EXE" "$EVENTS"
run_perf_stat "avx512_only" "$AVX512_EXE" "$EVENTS"

log "DONE"
log "Results: $OUTDIR"

