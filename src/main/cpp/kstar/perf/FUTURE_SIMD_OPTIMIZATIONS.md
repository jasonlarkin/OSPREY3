# SIMD / vectorization: planned optimizations (not implemented)

This note captures SIMD work that was investigated for `AStarSearchFast::expandInto` but is intentionally deferred until after the tutorial/testing work is in place.

## Target hotspot

Inside `AStarSearchFast::expandInto`, the core min-reduction loop is:

- For each `pos1 > k`
  - For each `rc1` at `pos1`
    - For each candidate `rc` at `k`:
      - `min_for_rc[rc] = min(min_for_rc[rc], base_e(rc1) + pairwise(pos1,rc1,k,rc))`

This loop is structurally friendly to SIMD on the `rc` dimension because it is:
- contiguous loads from the pairwise row (`row[rc]`)
- contiguous loads/stores for `min_for_rc[rc]`
- a pure `min` reduction with one broadcast (`base_e`) and one add

## Proposed implementation sketch

For `T=double` and AVX2:
- broadcast `base_e` into a `__m256d`
- process 4 `rc` values at a time:
  - `e4 = base4 + load(row + rc)`
  - `min4 = min(load(min_for_rc + rc), e4)`
  - store back
- scalar tail for `nrc_k % 4`

For `T=float` and AVX2:
- same structure with `__m256` and 8-wide vectors.

## Guardrails required before landing SIMD

- Keep a scalar reference path and compile-time guards (only enable when the toolchain guarantees support).
- Add a dedicated performance tutorial/test story so we can justify the optimization and detect regressions:
  - stable benchmark commands
  - correctness gate stays green
  - perf/annotate snippet identifying the exact loop as the bottleneck

## Expected benefit

On cases where `expandInto` dominates total time and `nrc_k` is large, SIMD should reduce the min-reduction inner-loop cost.
On cases where open-set churn dominates, SIMD may not move wall time meaningfully.

