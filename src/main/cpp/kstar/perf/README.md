# Performance / profiling notes (C++ K* port)

This folder is for **detailed, timestamped performance notes**: benchmark outputs, profiling results (`perf`, `callgrind`), and the conclusions/next steps that come from them.

Guiding rules:
- **Never trade away correctness**: before/after any perf change, run the Java-parity gates described in
  `src/main/cpp/kstar/TESTING_STRATEGY.md` (“Correctness gate for performance work”).
- Prefer **microbenchmarks + counters** first, then `perf record -g` to explain deltas.
- Keep outputs as **copy/pasteable logs** and include the exact command lines used.

Index:
- `2026-01-02_astar_and_pfunc_benchmarks.md`: initial A* baseline vs fast + partition function benchmark results, observations, and profiling next steps.

