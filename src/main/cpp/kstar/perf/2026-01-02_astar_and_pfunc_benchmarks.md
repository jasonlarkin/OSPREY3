# 2026-01-02 — A* baseline vs fast: benchmark results + interpretation

This note captures the initial microbenchmark runs for:
- **A\* “hot-loop” search** (pure open-set / expand / g/h cost; no partition-function math)
- **Partition function end-to-end** (A\* baseline vs fast vs GD)
- **Chrono runner** (sanity check / quick readouts)

It also documents the correctness gates that must stay green while iterating on performance.

## Environment

- **Run on**: WSL (Linux) on Windows host
- **CPU**: “Run on (4 X 2995.2 MHz CPU s)” (as reported by Google Benchmark)

## Benchmark command lines

All were run via the convenience targets (see `src/main/cpp/kstar/BENCHMARKING.md`):

```bash
cmake --build build/cpp/kstar --target kstar_run_astar_search_bench
cmake --build build/cpp/kstar --target kstar_run_partition_function_bench
cmake --build build/cpp/kstar --target kstar_run_partition_function_chrono
ctest --test-dir build/cpp/kstar --output-on-failure -R kstar\\.bench_
```

Important: Google Benchmark requires a **time suffix** on `--benchmark_min_time` (eg `0.5s`). We fixed our CMake targets to use suffixes.

## A* hot-loop benchmark (`kstar_astar_search_bench`)

Configuration:
- Cases: 2RL0 Protein/Ligand/Complex energy matrices
- Each run uses `max_pops=1000` to bound work and make variants comparable.

Output (Google Benchmark; **before** `EnergyMatrix` indexing optimization):

```text
BM_AStarBaseline_PopExpand/case:0/max_pops:1000/.../real_time    1792174 ns ... expands=160 leaves=840 max_open=1.852k max_pops=1k num_pos=4 pops=1k
BM_AStarBaseline_PopExpand/case:1/max_pops:1000/.../real_time    2250567 ns ... expands=143 leaves=857 max_open=1.676k max_pops=1k num_pos=4 pops=1k
BM_AStarBaseline_PopExpand/case:2/max_pops:1000/.../real_time   29255551 ns ... expands=777 leaves=223 max_open=7.797k max_pops=1k num_pos=8 pops=1k

BM_AStarFast_PopExpand/case:0/max_pops:1000/.../real_time         663482 ns ... expands=160 leaves=840 max_open=1.852k max_pops=1k num_pos=4 pops=1k
BM_AStarFast_PopExpand/case:1/max_pops:1000/.../real_time        1103973 ns ... expands=143 leaves=857 max_open=1.676k max_pops=1k num_pos=4 pops=1k
BM_AStarFast_PopExpand/case:2/max_pops:1000/.../real_time       35905767 ns ... expands=777 leaves=223 max_open=7.797k max_pops=1k num_pos=8 pops=1k
```

### Observations

- **Counters are consistent between baseline and fast** for the same case:
  - same `pops=1000`, same `expands/leaves/max_open`
  - that’s what we want: both variants are exploring the same search in the same order (first-unassigned expansion order).
- **Fast wins clearly for small position counts**:
  - case 0 (num_pos=4): ~0.66ms vs ~1.79ms
  - case 1 (num_pos=4): ~1.10ms vs ~2.25ms
- **Fast loses on case 2 (num_pos=8)** in the hot-loop benchmark:
  - ~35.9ms (fast) vs ~29.3ms (baseline)

## 2026-01-03 — Follow-up: O(1) EnergyMatrix indexing (fixing the perf hotspot)

### Root cause (from `perf report`, case:2)

When profiling the A* hot-loop (case:2), the dominant cost was **pairwise lookup overhead**:
- `EnergyMatrix<double>::getPairwise()`
- and especially `EnergyMatrix<double>::getPairwiseIndex()` plus validation (`validateConf`, `validatePos`)

This was expected to be expensive because the original `getPairwiseIndex()` computed offsets by **nested loops** over position pairs, i.e. **O(pos²)** work per lookup.

### Fix

Implemented in `EnergyMatrix`:
- Precompute `one_body_offsets_` and `pairwise_offsets_` in the constructor (prefix sums / triangular packing).
- Make `getOneBodyIndex()` and `getPairwiseIndex()` **O(1)** using those tables.
- Add hot-path accessors used by A* fast:
  - `getPairwiseAssumingPos1Greater(...)` (no swap, no validation)

### Note: this matches the original Java design

OSPREY Java already precomputes offsets in `edu.duke.cs.osprey.confspace.AbstractTupleMatrix`:
- `oneBodyOffsets[]` (prefix sums)
- `pairwiseOffsets[]` (triangular packed position pairs)

and its `getPairwiseIndex(res1,conf1,res2,conf2)` is O(1) using `pairwiseOffsets[...] + numConfAtPos[res2]*conf1 + conf2`.

So this C++ change is not “new behavior”, it is bringing our port’s indexing strategy in line with the Java baseline and removing an accidental O(pos²) per-lookup cost that the Java code avoids.

### A* hot-loop benchmark results after the fix

These were run after the change, with the same `max_pops=1000`:

```text
BM_AStarBaseline_PopExpand/case:2/max_pops:1000/min_time:0.500/real_time   17144078 ns ... expands=777 leaves=223 max_open=7.797k max_pops=1k num_pos=8 pops=1k
BM_AStarFast_PopExpand/case:2/max_pops:1000/min_time:0.500/real_time        4357560 ns ... expands=777 leaves=223 max_open=7.797k max_pops=1k num_pos=8 pops=1k
```

Interpretation:
- For **num_pos=8**, A* fast went from “loses” to **wins by ~4×** (≈17.1ms baseline vs ≈4.36ms fast for 1000 pops).
  This strongly supports “pairwise index/validation overhead” as the real bottleneck, not node representation.

### perf after the O(1) indexing change (case:2)

After the fix, `perf` shows the bottleneck moved:

#### Baseline (`BM_AStarBaseline_PopExpand.*case:2.*`)

Top lines (approx):

```text
15.71%  EnergyMatrix<double>::validateConf
13.93%  AStarSearch<double>::computeCachedEnergies
11.64%  EnergyMatrix<double>::getPairwise
11.16%  EnergyMatrix<double>::getPairwiseIndex
 8.13%  EnergyMatrix<double>::validatePos
```

Interpretation:
- Baseline still pays heavily for **validation + checked accessors**, even though the offset math is now O(1).

#### Fast (`BM_AStarFast_PopExpand.*case:2.*`)

Top lines (approx):

```text
36.81%  AStarSearchFast<double>::computeHScore
24.29%  EnergyMatrix<double>::getPairwiseAssumingPos1Greater
 7.06%  EnergyMatrix<double>::getOneBodyIndex
 6.03%  AStarSearchFast<double>::computeGScore
 4.78%  EnergyMatrix<double>::validateConf
```

Interpretation:
- In fast, indexing overhead is largely gone; remaining cost is dominated by **heuristic (H-score)** and the raw volume of pairwise accesses.
- The remaining `validate*`/`getOneBodyIndex` time suggests an obvious next micro-optimization: use `getOneBodyUnchecked()` in A* hot loops.

### perf after removing validation from the A* fast hot loops (case:2)

After switching A* fast to:
- `EnergyMatrix::getOneBodyUnchecked()` (instead of `getOneBody()`),
- and keeping `getPairwiseAssumingPos1Greater()` in the tight loops,

`perf` for `BM_AStarFast_PopExpand/case:2/max_pops:1000` shows:

```text
41.50%  AStarSearchFast<double>::computeHScore
27.61%  EnergyMatrix<double>::getPairwiseAssumingPos1Greater
10.57%  runOneSearch<AStarNodeFast<double>, AStarSearchFast<double>>
 9.25%  AStarSearchFast<double>::computeGScore
 4.22%  EnergyMatrix<double>::getOneBodyUnchecked
 1.86%  AStarSearchFast<double>::expand
 1.35%  std::vector<AStarNodeFast<double>>::_M_realloc_insert(...)
```

Interpretation:
- The next ceiling is now clearly `AStarSearchFast::computeHScore` + the remaining pairwise access volume.
- With validation removed, further wins likely require improving the **H-score computation strategy** (not just index math):
  - reduce redundant work when expanding many children of the same parent
  - reduce branchiness / scanning in the “assigned/unassigned” loops
  - consider incremental/cached updates to the heuristic across expansions

### Hypotheses for why “fast” loses at num_pos=8

This is the key result to explain with profiling before changing code.

Plausible contributors:
- **Node copy / move cost**: `AStarNodeFast` stores both inline and heap members; copying may be heavier than expected, especially when the open set gets large (`max_open ~ 7.8k`).
- **Branchiness**: `AStarSearchFast::computeHScore` does multiple loops guarded by `if (a[pos] >= 0)` / `if (a[pos2] < 0)`; at higher pos counts this may become more branch-mispredict heavy.
- **Priority queue churn**: for larger open sets, time may be dominated by `std::priority_queue` pushes/pops and comparator cost rather than g/h arithmetic.

### Next profiling steps (to turn hypotheses into facts)

Run `perf` on the slow case only (case 2) to isolate hotspots:

```bash
perf record -g -- ./build/cpp/kstar/kstar_astar_search_bench --benchmark_filter=BM_AStar.*case:2.* --benchmark_min_time=2.0s
perf report
```

#### Important: avoid “Missing test data …” during perf runs

The benchmark resolves `.emat.bin` inputs via `OSPREY_KSTAR_TEST_DATA_DIR` or by searching upward from the **current working directory**
(see `src/test/cpp/kstar/test_data_paths.hpp`). If you run `perf record` from repo root, it may not find `build/cpp/kstar/test_data`.

Two reliable options:

1) Run from the build directory:

```bash
cd build/cpp/kstar
perf record -g -- ./kstar_astar_search_bench --benchmark_filter=BM_AStar.*case:2.* --benchmark_min_time=2.0s
perf report
```

2) Or set the env var explicitly:

```bash
export OSPREY_KSTAR_TEST_DATA_DIR=$PWD/build/cpp/kstar/test_data
perf record -g -- ./build/cpp/kstar/kstar_astar_search_bench --benchmark_filter=BM_AStar.*case:2.* --benchmark_min_time=2.0s
perf report
```

Then compare baseline vs fast:
- which spends more in `computeHScore`, `expand`, node copying, and priority queue internals?
- check whether the “fast” node representation is causing more copies/moves (or larger object size hurts cache).

## Partition function benchmark (`kstar_partition_function_bench`)

Output (Google Benchmark):

```text
BM_Pfunc_AStarBaseline/case:0 ...  730988 ns ... num_confs_eval=49  num_pos=4
BM_Pfunc_AStarBaseline/case:1 ... 1240154 ns ... num_confs_eval=244 num_pos=4
BM_Pfunc_AStarBaseline/case:2 ... 97731170 ns ... num_confs_eval=1.361k num_pos=8
...
BM_Pfunc_AStarFast/case:0 ...  349854 ns ...
BM_Pfunc_AStarFast/case:1 ...  689456 ns ...
BM_Pfunc_AStarFast/case:2 ... 80424219 ns ...
...
BM_Pfunc_GD/case:2 ... 92233898 ns ... num_confs_eval=34 num_pos=8
```

### Observations

- End-to-end, **AStarFast is often faster** than baseline, including on some larger cases.
- The A* hot-loop result (“fast loses at num_pos=8”) vs the end-to-end result (“fast wins at case 2”) implies:
  - the “fast” wins may be coming from differences in *overall partition-function loop behavior* (eg how many leaf energies are fully evaluated before convergence), or cache effects across the full compute, not just open-set mechanics.
  - we should be careful to optimize based on the metric we care about:
    - **pure search throughput** vs
    - **time-to-epsilon convergence**.

## Chrono runner (`kstar_partition_function_chrono`)

Chrono is useful for quick “smell tests,” but it can be noisy at low reps.
For stable comparisons, increase `--reps` significantly (eg 30+).

Example output excerpt:

```text
2RL0_Protein AStarBaseline reps=3 total_ms=1.604 per_ms=0.535 ...
2RL0_Protein AStarFast     reps=3 total_ms=3.678 per_ms=1.226 ...
```

This contradicts Google Benchmark (which showed AStarFast faster on small cases), and is likely explained by:
- very small rep count (`reps=3`)
- turbo/frequency changes
- cache warmness / execution order effects

Recommendation: treat chrono output as **directional** unless `reps` is high.

## Correctness gate for performance work (Java baseline parity)

Before and after any performance refactor to A\*, ConfSearch, or partition function logic, run:

1. **C++ VERBATIM partition function tests** (Java-parity thresholds):

```bash
ctest --test-dir build/cpp/kstar --output-on-failure -R ^PartitionFunction_VERBATIM\\.
```

2. **C++ EnergyMatrix Java comparison tests** (loader + semantics parity):

```bash
ctest --test-dir build/cpp/kstar --output-on-failure -R ^EnergyMatrix_JavaComparison\\.
```

3. Optionally, the full “verbatim umbrella”:

```bash
ctest --test-dir build/cpp/kstar --output-on-failure -R ^kstar\\.verbatim_all$
```

If the `.emat.bin` test data is missing/outdated, regenerate via the Java exporter referenced by the tests (see the skip messages in `src/test/cpp/kstar/test_partition_function_gtest.cpp`).

## Correctness gate status (this run)

You ran the gate after the benchmark work and it is **green**:

- **`PartitionFunction_VERBATIM.*`**:
  - **Passed**: all CPU tests
  - **Skipped**: GPU placeholders (expected) + `RL0_Ligand_WithConfDB_GD` (expected placeholder)
  - Summary: “100% tests passed, 0 tests failed out of 41”

- **`EnergyMatrix_JavaComparison.*`**:
  - Summary: “100% tests passed, 0 tests failed out of 8”

- **`kstar.verbatim_all`**:
  - Summary: “100% tests passed, 0 tests failed out of 1”
  - Runtime: ~7.7s (as reported by CTest)
