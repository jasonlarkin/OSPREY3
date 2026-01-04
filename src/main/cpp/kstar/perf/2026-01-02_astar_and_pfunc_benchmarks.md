# 2025-12-28 — A* baseline vs fast: benchmark results + interpretation

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

Important: Google Benchmark requires a **time suffix** on `--benchmark_min_time` (eg `0.5s`). Fixed CMake targets to use suffixes.

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
  - both variants are exploring the same search in the same order (first-unassigned expansion order).
- **Fast wins clearly for small position counts**:
  - case 0 (num_pos=4): ~0.66ms vs ~1.79ms
  - case 1 (num_pos=4): ~1.10ms vs ~2.25ms
- **Fast loses on case 2 (num_pos=8)** in the hot-loop benchmark:
  - ~35.9ms (fast) vs ~29.3ms (baseline)

## 2025-12-28 — Follow-up: O(1) EnergyMatrix indexing (fixing the perf hotspot)

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

## 2025-12-29 — Follow-up: incremental / batched H-score and open-set allocation wins

This section captures subsequent micro-optimizations to the **fast** A* variant that preserve semantics
but reduce redundant work and allocation churn.

### Correctness gate (must stay green)

All performance changes below were kept behind the same correctness gates:

```bash
ctest --test-dir build/cpp/kstar --output-on-failure -R '^AStarSearch_SYNTHESIZED\.SearchBasicOperations$'
ctest --test-dir build/cpp/kstar --output-on-failure -R ConfSearchAStar_SYNTHESIZED
ctest --test-dir build/cpp/kstar --output-on-failure -R '^PartitionFunction_VERBATIM\.'
ctest --test-dir build/cpp/kstar --output-on-failure -R '^EnergyMatrix_JavaComparison\.'
ctest --test-dir build/cpp/kstar --output-on-failure -L gtest
```

### Change 1: eliminate redundant H-score recomputation across sibling children

In `AStarSearchFast::expand(...)`, previously computed `computeHScore(child)` for each child, which
repeated most of the same work per sibling.

Refactored expand to compute:
- shared, per-(pos1,rc1) “base” terms once per expansion, and
- per-child terms using only the interaction with the newly-assigned `(k,rc)`.

To keep this allocation-free in the hot loop, switched to reusable scratch buffers in `AStarSearchFast`.

### Change 2: add `EnergyMatrix` row view + bulk child H computation

Added a hot-path accessor:
- `EnergyMatrix::getPairwiseRowAssumingPos1Greater(pos1, conf1, pos2) -> std::span<const T>`

This exposes a contiguous row of `pairwise(pos1,conf1,pos2,conf2)` values for all `conf2` and allows
`AStarSearchFast::expand` to compute all child H-scores in bulk with better cache locality and fewer
per-element function calls.

### Change 3: reserve open-set backing storage in the benchmark harness

`perf report` showed `std::vector::_M_realloc_insert` due to priority-queue growth.
In `bench_astar_search_google_benchmark.cpp`, now reserve the `std::vector` backing store used by
`std::priority_queue` to reduce reallocations and allocator noise during the hot loop.

### Benchmark result (case:2, max_pops=1000)

Run (with repetitions/aggregates to reduce noise):

```bash
./build/cpp/kstar/kstar_astar_search_bench \
  --benchmark_filter=case:2 \
  --benchmark_min_time=1s \
  --benchmark_repetitions=5 \
  --benchmark_report_aggregates_only=true
```

Observed on the same WSL host class (“Run on (4 X 2995.2 MHz CPU s)”):

```text
BM_AStarBaseline_PopExpand/case:2/.../real_time_mean      ~9.61 ms   (CV ~15%)
BM_AStarFast_PopExpand/case:2/.../real_time_mean         ~1.02 ms   (CV ~4.7%)
```

Counters remained identical (`pops=1000`, `expands=777`, `leaves=223`, `max_open=7.797k`), which is the
critical evidence that the optimization did not change search behavior.

### perf summary (fast, case:2)

After these changes, the dominant costs are still in the A* hot loop, but allocator noise is no longer
prominent. Representative top lines include:

```text
~37%  AStarSearchFast<double>::expand
~17%  EnergyMatrix<double>::getPairwiseAssumingPos1Greater
~3-4% AStarSearchFast<double>::sumUndefinedRange
~2-3% EnergyMatrix<double>::getOneBodyUnchecked
~2%   EnergyMatrix<double>::getPairwiseRowAssumingPos1Greater
```

Interpretation:
- The next ceiling is now **expand + raw pairwise access volume**, not indexing/validation or open-set allocation.

## 2025-12-29 — Follow-up: reducing pairwise accessor overhead inside expand()

## Benchmark workload sizes (P and r_i) and how this relates to “# atoms”

When reading perf/benchmark results, it’s important to distinguish two different “size” notions:

- **Atom count** (eg “500–17k atoms” in OSPREY examples) mainly affects the **cost to compute energies**
  (forcefield/score evaluation, minimization, energy function plumbing) *before* an energy matrix exists.
- **EnergyMatrix / K\* search size** is governed by:
  - **P = number of design positions**
  - **r_i = number of rotamers/conformations at position i**

For an `EnergyMatrix` (as used by our A\* hot-loop microbenchmarks), the key derived counts are:

- **1-body term count**: \( \sum_i r_i \)
- **2-body term count**: \( \sum_{i>j} r_i r_j \)

These counts define the size of the precomputed matrix and heavily influence search throughput, independent
of how many atoms were in the original structure used to generate the energies.

### Current profiling benchmark shapes (from `scripts/inspect_emat_bin.py`)

The A\* hot-loop benchmark inputs we’ve been profiling are the 2RL0 matrices:

- **2RL0 Protein**
  - `P=4`, `r_i=[5,6,9,19]`
  - `oneBodyTerms=sum(r_i)=39`
  - `pairwiseTerms=sum_{i>j}(r_i*r_j)=509`

- **2RL0 Ligand**
  - `P=4`, `r_i=[5,28,8,19]`
  - `oneBodyTerms=60`
  - `pairwiseTerms=1183`

- **2RL0 Complex** (**case:2** in the benchmark; main perf target)
  - `P=8`, `r_i=[5,28,8,19,5,6,9,19]`
  - `oneBodyTerms=99`
  - `pairwiseTerms=4032`

Interpretation:
- These are **small P** (4–8 positions) but moderately skewed `r_i` (some positions have 19–28 RCs).
- That’s enough to produce a non-trivial open-set and inner-loop workload, but it’s still far from the largest
  possible production design spaces.

### Motivation

`perf report` for the fast A* hot loop still showed substantial time in:
- `EnergyMatrix<double>::getPairwiseAssumingPos1Greater`

even after earlier indexing/validation fixes.

### Change

Introduced a lower-level view:
- `EnergyMatrix::getPairwiseBlockAssumingPos1Greater(pos1,pos2)` returning `{data,n1,n2}`

and rewrote `AStarSearchFast::expand` to consume these blocks directly (pointer + stride)
to batch:
- adding pairwise energies for assigned positions, and
- computing child g-scores across all RCs,
without repeated function calls in inner loops.

### Result (case:2)

Representative benchmark output (mean across 5 repetitions):

```text
BM_AStarBaseline_PopExpand/case:2/.../real_time_mean  ~10.06 ms
BM_AStarFast_PopExpand/case:2/.../real_time_mean     ~0.81 ms
```

Representative perf top lines for `BM_AStarFast_PopExpand/case:2/max_pops:1000`:

```text
~42%  AStarSearchFast<double>::expand
~3.5% AStarSearchFast<double>::sumUndefinedRangeUnchecked
~2.8% EnergyMatrix<double>::getOneBodyUnchecked
~2.0% EnergyMatrix<double>::getPairwiseBlockAssumingPos1Greater
```

Interpretation:
- Pairwise accessor overhead moved off the top lines, and the dominant cost is now inside `expand` itself
  (the remaining min-reduction work across RCs).

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
  - should be careful to optimize based on the metric:
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

## 2025-12-29: Fast A* hot-loop refactor (eliminate per-expand allocations)

### Change

The A* benchmark harness (`kstar_astar_search_bench`) was previously allocating a fresh `std::vector` on every expansion:
- `auto children = search.expand(node);`

For the fast implementation, this meant per-expand heap activity even though the algorithm itself is written to avoid per-expand allocations.

Fix:
- Add `AStarSearchFast::expandInto(node, out)` to expand into caller-provided storage.
- Update the benchmark loop to reuse a single `children_scratch` vector per search run when `expandInto` is available.

### Benchmark impact (largest built-in case)

Case: `case:3` (`test_data/2RL0.complex.emat.bin`), `max_pops=1000`

- Baseline: ~40.5ms
- Fast (post-change): ~1.93ms (observed; run-to-run varies with load)

This is a large step-change and suggests the benchmark harness allocation was materially inflating the measured “fast” cost.

### Updated perf top lines (fast, case:3)

With the new hot loop, perf attributes time primarily to:
- `osprey::kstar::AStarSearchFast<double>::expandInto` (still the dominant hotspot)
- `AStarSearchFast<double>::sumUndefinedRangeUnchecked`
- `EnergyMatrix<double>::getOneBodyUnchecked`
- `EnergyMatrix<double>::getPairwiseBlockAssumingPos1Greater`

Next action: use `perf annotate` on `expandInto` to identify which inner loop(s) dominate (min-reduction vs base accumulation vs child g-score bulk update).

## 2026-01-04: Stabilized case:3 baseline vs fast numbers (largest built-in)

Ran with pinned core and longer min time to reduce noise:

```bash
cd build/cpp/kstar
taskset -c 0 ./kstar_astar_search_bench \
  --benchmark_filter='BM_AStarBaseline_PopExpand/case:3|BM_AStarFast_PopExpand/case:3' \
  --benchmark_min_time=5s \
  --benchmark_repetitions=10 \
  --benchmark_report_aggregates_only=true
```

Results (case:3, `max_pops=1000`, `pops=1000`, `expands=994`, `leaves=6`, `max_open=26.756k`, `num_pos=7`):

```text
BM_AStarBaseline_PopExpand/case:3/.../real_time_mean   36.33 ms   (CV 19.34%)
BM_AStarFast_PopExpand/case:3/.../real_time_mean       1.73 ms   (CV  0.54%)
```

Interpretation:
- Fast is ~21× faster on mean (case:3) with tight variance.
- Baseline still exhibits significant variance on this host, even pinned (likely allocator/heap churn and cache effects).

Note on `min_time:0.500` in the printed benchmark name:
- The benchmark is registered in code with `->MinTime(0.5)`, so Google Benchmark prints `min_time:0.500` even when running with a higher `--benchmark_min_time` flag.
  Use the wall time + repetitions/CV as the stability indicator.
