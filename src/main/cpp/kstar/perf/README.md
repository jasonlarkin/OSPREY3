# Performance / profiling notes (C++ K* port)

This folder is for **detailed, timestamped performance notes**: benchmark outputs, profiling results (`perf`, `callgrind`), and the conclusions/next steps that come from them.

Guiding rules:
- **Never trade away correctness**: before/after any perf change, run the Java-parity gates described in
  `src/main/cpp/kstar/TESTING_STRATEGY.md` (“Correctness gate for performance work”).
- Prefer **microbenchmarks + counters** first, then `perf record -g` to explain deltas.
- Keep outputs as **copy/pasteable logs** and include the exact command lines used.

Index:
- `2026-01-02_astar_and_pfunc_benchmarks.md`: initial A* baseline vs fast + partition function benchmark results, observations, and profiling next steps.
- `DIRECT_CPP_VS_JAVA_COMPARISON.md`: direct comparison artifact definition + one-command runner for C++ benchmark JSON + Java benchmark logs.
- `STRUCT_LAYOUT_AND_ASTAR_NODE_DESIGN.md`: struct layout (padding/alignment/cache lines) applied to A* node representation; measurement + experiment checklist.
- `PROFILING_SNIPPETS.md`: raw excerpts from `gdb` / `perf` used as cited artifacts in the docs.
- `ROOFLINE_NOTES.md`: expandInto roofline model notes, plotting workflow, and how to interpret points above the “roofline”.

