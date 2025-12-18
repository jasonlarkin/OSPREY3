#!/bin/bash
# Profile cache hierarchy + core perf counters for rotation/normalize benchmarks across example-driven sizes.
#
# Output:
#   ./tmp/rotation_cache_hierarchy.csv
#   ./tmp/rotation_cache_perf_<op>_<impl>_<n>.txt (raw perf output per run)
#
# Requires: perf, python3

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$REPO_ROOT"

BENCH_DIR="$REPO_ROOT/src/main/cc/ConfEcalc"
BENCH_BIN="$BENCH_DIR/build/benchmark_rotation_operations"

mkdir -p "$REPO_ROOT/tmp"

# If perf isn't installed/supported (common on WSL without linux-tools),
# leave the CSV with header only and exit cleanly.
if ! command -v perf >/dev/null 2>&1; then
  OUT_CSV="$REPO_ROOT/tmp/rotation_cache_hierarchy.csv"
  echo "op,impl,num_vectors,iterations,cycles,instructions,ipc,task_clock_ms,cache_references,cache_misses,cache_miss_rate,L1_loads,L1_load_misses,L1_miss_rate,LLC_loads,LLC_load_misses,LLC_miss_rate" > "$OUT_CSV"
  echo "perf not found; skipping rotation cache hierarchy profiling (wrote header only): $OUT_CSV" >&2
  exit 0
fi

# Probe perf support for this kernel (WSL often requires linux-tools-<kernel>).
if perf stat -x, -e cycles -- true 2>&1 | grep -qi "perf not found for kernel"; then
  OUT_CSV="$REPO_ROOT/tmp/rotation_cache_hierarchy.csv"
  echo "op,impl,num_vectors,iterations,cycles,instructions,ipc,task_clock_ms,cache_references,cache_misses,cache_miss_rate,L1_loads,L1_load_misses,L1_miss_rate,LLC_loads,LLC_load_misses,LLC_miss_rate" > "$OUT_CSV"
  echo "perf unsupported for this kernel; skipping rotation cache hierarchy profiling (wrote header only): $OUT_CSV" >&2
  exit 0
fi

# Build
cd "$BENCH_DIR"
cmake -B build -DENABLE_SIMD=ON >/dev/null
cmake --build build --target benchmark_rotation_operations >/dev/null
cd "$REPO_ROOT"

if [ ! -x "$BENCH_BIN" ]; then
  echo "ERROR: benchmark not found: $BENCH_BIN" >&2
  exit 1
fi

# Derive representative sizes from examples PDB atom counts (percentiles)
# Fallback sizes are used if examples can't be parsed.
SIZES_JSON=$(python3 - <<'PY'
import json
from pathlib import Path
import numpy as np

repo = Path(".").resolve()
examples = repo / "examples"

def count_atoms(p):
    c = 0
    try:
        with p.open("r", errors="ignore") as f:
            for line in f:
                if line.startswith(("ATOM  ", "HETATM")):
                    c += 1
    except Exception:
        return None
    return c if c > 0 else None

atom_counts = []
if examples.exists():
    for pdb in examples.rglob("*.pdb"):
        n = count_atoms(pdb)
        if n is not None:
            atom_counts.append(n)

atom_counts = sorted(atom_counts)
if len(atom_counts) >= 10:
    # Include tails explicitly to cover the full range seen in examples
    q = [0.00, 0.10, 0.25, 0.50, 0.75, 0.90, 0.95, 0.99, 1.00]
    sizes = [int(np.quantile(atom_counts, p)) for p in q]
    # keep unique, clamp to sane minimum
    sizes = sorted({max(16, s) for s in sizes})
else:
    sizes = [50, 150, 300, 500, 1000]

print(json.dumps(sizes))
PY
)

# Convert sizes list into bash array
readarray -t SIZES < <(python3 - <<PY
import json
for s in json.loads('''$SIZES_JSON'''):
    print(s)
PY
)

# Pick iterations so each run is measurable but not huge (O(n) per iter)
# Target ~2e8 "vector ops" per run, capped.
iterations_for_size() {
  local n="$1"
  python3 - <<PY
n = int($n)
target = 200_000_000
iters = max(10, min(2000, target // max(1, n)))
print(iters)
PY
}

EVENTS="cycles:u,instructions:u,cache-references:u,cache-misses:u,L1-dcache-loads:u,L1-dcache-load-misses:u,LLC-loads:u,LLC-load-misses:u,task-clock:u"

OUT_CSV="$REPO_ROOT/tmp/rotation_cache_hierarchy.csv"
echo "op,impl,num_vectors,iterations,cycles,instructions,ipc,task_clock_ms,cache_references,cache_misses,cache_miss_rate,L1_loads,L1_load_misses,L1_miss_rate,LLC_loads,LLC_load_misses,LLC_miss_rate" > "$OUT_CSV"

run_perf_one() {
  local op="$1"
  local impl="$2"
  local n="$3"
  local iters="$4"

  local raw="$REPO_ROOT/tmp/rotation_cache_perf_${op}_${impl}_${n}.txt"

  # Use perf stat CSV output for stable parsing
  # NOTE: perf writes stats to stderr; capture all output.
  local perf_out
  perf_out=$(perf stat -x, -e "$EVENTS" -- "$BENCH_BIN" "$n" "$iters" --op "$op" --impl "$impl" 2>&1)
  echo "$perf_out" > "$raw"

  python3 - <<PY
import re
from collections import defaultdict

txt = open(r"$raw","r",errors="ignore").read().splitlines()
vals = {}

def parse_num(s):
    s = s.strip()
    if s in ("", "<not supported>", "not supported", "<not counted>"):
        return None
    s = s.replace(",", "")
    try:
        return float(s)
    except Exception:
        return None

# Normalize perf event names: perf may emit events without ":u" suffix even if requested.
def norm_event(ev: str) -> str:
    ev = ev.strip()
    if ev.endswith(":u"):
        ev = ev[:-2]
    return ev

# perf -x, format: value,unit,event,run,enabled, ...
for line in txt:
    parts = [p.strip() for p in line.split(",")]
    if len(parts) < 3:
        continue
    v, unit, ev = parts[0], parts[1], parts[2]
    if not ev or ev.startswith("#"):
        continue
    key = norm_event(ev)
    num = parse_num(v)
    if num is None:
        # record unsupported explicitly if not already present
        if key not in vals:
            vals[key] = None
        continue
    if key not in vals:
        vals[key] = num

cycles = vals.get("cycles", 0.0) or 0.0
insns = vals.get("instructions", 0.0) or 0.0
task_clock = vals.get("task-clock", 0.0) or 0.0  # ms

cache_refs = vals.get("cache-references", 0.0) or 0.0
cache_miss = vals.get("cache-misses", 0.0) or 0.0
l1_loads = vals.get("L1-dcache-loads", 0.0)
l1_miss = vals.get("L1-dcache-load-misses", 0.0)
llc_loads = vals.get("LLC-loads", 0.0)
llc_miss = vals.get("LLC-load-misses", 0.0)

ipc = (insns / cycles) if cycles > 0 else 0.0
cache_miss_rate = (cache_miss / cache_refs * 100.0) if cache_refs > 0 else 0.0

def i_or_missing(v):
    return int(v) if v is not None else -1

def rate_or_missing(num, den):
    if num is None or den is None:
        return -1.0
    if den > 0:
        return (num / den) * 100.0
    return 0.0

l1_miss_rate = rate_or_missing(l1_miss, l1_loads)
llc_miss_rate = rate_or_missing(llc_miss, llc_loads)

print(",".join(map(str, [
    "$op", "$impl", $n, $iters,
    int(cycles), int(insns), f"{ipc:.4f}", f"{task_clock:.3f}",
    int(cache_refs), int(cache_miss), f"{cache_miss_rate:.4f}",
    i_or_missing(l1_loads), i_or_missing(l1_miss), f"{l1_miss_rate:.4f}",
    i_or_missing(llc_loads), i_or_missing(llc_miss), f"{llc_miss_rate:.4f}",
])))
PY
}

echo "=== Profiling rotation cache hierarchy ===" >&2
echo "Sizes (from examples): ${SIZES[*]}" >&2
echo "Output CSV: $OUT_CSV" >&2

for n in "${SIZES[@]}"; do
  iters="$(iterations_for_size "$n")"
  echo "Size: $n  Iterations: $iters" >&2

  for op in rotation normalize; do
    # scalar
    run_perf_one "$op" scalar "$n" "$iters" >> "$OUT_CSV"

    # simd (only meaningful if compiled with USE_SIMD)
    run_perf_one "$op" simd "$n" "$iters" >> "$OUT_CSV"
  done
done

echo "Done: $OUT_CSV" >&2

echo "" >&2
echo "=== Summary (scalar vs simd) ===" >&2
python3 "$REPO_ROOT/scripts/tools/summarize_rotation_cache_hierarchy.py" | head -200 >&2


