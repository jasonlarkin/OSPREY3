# A* Implementation Notes (C++ K* Phase‑1)

This file exists to justify (and document) why Phase‑1 uses a **custom A*** implementation rather than adopting an external library.

## Current state (what exists now)

Phase‑1 already includes two A* implementations over an `EnergyMatrix`:

- **Baseline A***: simple and readable reference implementation.
- **Fast A***: optimized variant intended for performance work.

Both variants are required to be behaviorally identical at the partition‑function layer and are covered by tests.

## Why an external A* library is not a fit

Most off‑the‑shelf A* libraries target grid/path planning and do not match this problem shape:

- The “graph” is **implicit** (conformation assignment tree), not an explicit adjacency list.
- The heuristic is **domain‑specific** (EnergyMatrix pairwise‑min bounds).
- Performance is dominated by **node expansion + scoring**, not generic graph plumbing.

## What to look at in this repo

- **Baseline A***:
  - `src/main/cpp/kstar/astar_node.hpp`
  - `src/main/cpp/kstar/astar_search.hpp`
  - `src/main/cpp/kstar/astar_search.cpp`
- **Fast A***:
  - `src/main/cpp/kstar/astar_node_fast.hpp`
  - `src/main/cpp/kstar/astar_search_fast.hpp`
  - `src/main/cpp/kstar/astar_search_fast.cpp`
- **Correctness mapping + test boundaries**:
  - `src/main/cpp/kstar/CORRECTNESS_AND_CXX20_RATIONALE.md`

## Practical guidance

- Keep **Baseline** as the readable reference for profiling deltas and regression triage.
- Add optimizations only in **Fast** and enforce equivalence via tests (`PartitionFunction_AStarVariants.*`).

