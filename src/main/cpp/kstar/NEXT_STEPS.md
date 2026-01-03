# Next Implementation Steps

## Current status (Phase‑1)

The earlier “blocking issues” (overflow, A* ordering, and test‑data mismatches) have been addressed in the current Phase‑1 implementation:

- Partition function accumulation is done in **log10 space** (numeric stability).
- A* ordering and correctness are validated by CTests, including **Baseline vs Fast** equivalence.
- “Verbatim” partition‑function tests use **Java-exported EnergyMatrices that match the Java ConfSpace setup** (no CCSX vs SimpleConfSpace mismatch).

## What to do next (high-signal work only)

### 1) Commit hygiene + documentation pruning

- Keep the Phase‑1 commit focused on: **C++ code + tests + a small set of up‑to‑date docs**.
- Treat per‑file “local reference” markdowns as local-only.

### 2) Java ↔ C++ performance comparison (meaningful benchmark split)

- Use the Java benchmark runner (`KStarPfuncBenchmark`) to report separate:
  - `emat_ms` (load/compute)
  - `setup_ms`
  - `per_ms` (pfunc loop)
- Compare C++ timings against Java `per_ms` only (Phase‑1 is emat-only).

### 3) Phase‑2 boundary: ConfSpace + real energy pipeline

Phase‑1 correctness is EnergyMatrix-only. The next major step is defining (and testing) the ConfSpace/JNA boundary required for:

- Continuous minimization parity
- Full K* end-to-end correctness vs Java `TestKStar`

### 4) Expand precision tiers (synthetic, deterministic)

Use `PRECISION_TESTING.md` as the canonical roadmap; continue adding:

- more Tier‑0 oracle comparisons (exact enum oracle vs A* variants)
- more Tier‑1 metamorphic invariants (epsilon monotonicity, determinism, const‑term shifts)
