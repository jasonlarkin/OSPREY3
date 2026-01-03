# Phase‑1 Minimal K* (what Phase‑1 is, and what it is not)

Phase‑1 is intentionally scoped to **EnergyMatrix-only** correctness and performance work.
It provides a C++ implementation of:

- `EnergyMatrix<T>` + loader for a Java-exported binary format
- Partition function bounds (`PartitionFunction<T>`) using log-space math
- A* search variants (Baseline + Fast) for the partition function
- Supporting building blocks (ConfIndex, ConfRanker, ConfSearchCache, KStarScore, thread pool utilities)

## What Phase‑1 does not attempt

- No ConfSpace/JNA integration as a runtime dependency for correctness (Phase‑2+ boundary)
- No continuous minimization parity with Java’s energy pipeline (Phase‑2+ boundary)
- No “full K*” end-to-end parity against `TestKStar` (Phase‑2+)

## Boundary with Java (how we compare today)

Java is used to export deterministic artifacts; C++ consumes them:

- Java test/export code writes `*.emat.bin` (and for some suites `*.rcs.bin`, expected scored confs).
- C++ loads those artifacts and runs verbatim-style bound contract tests.

See `src/main/cpp/kstar/CORRECTNESS_AND_CXX20_RATIONALE.md` for the canonical diagram and boundary contracts.

## Phase‑2+ (explicit next boundary)

To move beyond emat-only parity, Phase‑2 needs:

- A concrete ConfSpace interface into C++ (JNA or native loader)
- A real energy/ref-energy/minimization pipeline in C++ (or a hybrid that is still measurable/comparable)
- End-to-end K* score parity tests vs Java `TestKStar`