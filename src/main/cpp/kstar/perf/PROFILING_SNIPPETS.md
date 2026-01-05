# Profiling snippets (copy/paste artifacts)

This file captures raw excerpts from profiling/layout tools that are referenced by the performance docs.

## `gdb ptype /o`: `AStarNodeFast<double>` layout (post-change)

Command:

```bash
gdb -q build/cpp/kstar-perf/kstar_astar_search_bench \
  -ex 'set pagination off' \
  -ex 'ptype /o osprey::kstar::AStarNodeFast<double>' \
  -ex quit
```

Output excerpt:

```text
/* offset      |    size */  type = struct osprey::kstar::AStarNodeFast<double> [with T = double] {
/*      0      |      32 */    ... inline_assignments;
/*     32      |       8 */    int16_t *heap_assignments;
/*     40      |       4 */    int32_t num_positions;
/*     44      |       1 */    bool uses_heap;
/* XXX  3-byte hole      */
/*     48      |       8 */    T g_score;
/*     56      |       8 */    T h_score;
/*     64      |       8 */    T f_score;
/*     72      |       4 */    int32_t level;
/* XXX  4-byte padding   */
/* total size (bytes):   80 */
}
```

## `perf` rerun after node-layout change (`BM_AStarFast_PopExpand/case:2`)

Benchmark command (pinned CPU0):

```bash
cd build/cpp/kstar-perf
taskset -c 0 ./kstar_astar_search_bench \
  --benchmark_filter='^BM_AStarFast_PopExpand/case:2/' \
  --benchmark_repetitions=5 \
  --benchmark_min_time=500ms \
  --benchmark_report_aggregates_only=true
```

Representative benchmark output excerpt:

```text
BM_AStarFast_PopExpand/case:2/.../real_time_mean       599283 ns
BM_AStarFast_PopExpand/case:2/.../real_time_cv           9.38 %
```

`perf report` excerpt:

```text
42.09%  [.] osprey::kstar::AStarSearchFast<double>::expandInto
  |
  |--29.66%--osprey::kstar::AStarSearchFast<double>::expandInto
  |
  |--4.03%--_mm256_storeu_pd (inlined)
```

## DRAM-scale synthetic roofline test (CTest)

The roofline microbenchmark includes a DRAM-scale synthetic point:

- `num_pos=96, rc=64` (pairwise payload alone is ~149 MB)

CTest entrypoint (perf build tree):

```bash
ctest --test-dir build/cpp/kstar-perf --output-on-failure -R '^kstar\.bench_mem\.expandinto_roofline_96$'
```
