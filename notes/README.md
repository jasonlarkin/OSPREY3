## notes/

Project documentation that is useful for ongoing development, organized by topic.

Current layout:
- `notes/simd/`: SIMD implementation notes, investigations, dispatch/versioning, performance bottlenecks
- `notes/benchmarks/`: benchmarking plans + results writeups
- `notes/profiling/`: perf/roofline/cache hierarchy notes and profiling guides
- `notes/modern/`: modernization planning notes (C++20/23, etc.)

This directory is **committed**. If local-only scratch is needed, use:
- `notes/.scratch/` or `notes/.local/` (both gitignored; see `.gitignore`)

The canonical reproducibility runbook for SIMD results is kept in repo root:
- `SIMD_REPRODUCIBILITY.md`


