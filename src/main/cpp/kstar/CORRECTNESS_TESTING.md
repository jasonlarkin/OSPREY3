## Correctness Testing (C++ K* Port)

This document describes correctness-focused testing layers for the C++ K* port, and where each layer lives in the test suite.

Scope:

- Correctness of **A***-based conformation search and **A***-based partition function bounds.
- Correctness of **partition function** computation and its numeric/bounds invariants.
- How to add high-ROI correctness tests without requiring Java-exported fixtures.

Related docs:

- `CODE_COVERAGE.md`: how to measure/report coverage (lcov/llvm-cov) and quantify fuzz ROI.
- `COVERAGE_GUIDED_TESTING.md`: how to generate new inputs (fuzz/symbolic) that buy coverage and promote them to regressions.
- `PRECISION_TESTING.md`: tiered precision tests (oracle + invariants) and how to run them.
- `CORRECTNESS_AND_CXX20_RATIONALE.md`: high-level rationale/index for correctness concerns.

### Categories (what to add first)

#### 0) Crash-safety tests (sanitizers + fuzzing + corpus replay)

Goal: catch memory errors/UB and make fuzz results actionable and repeatable.

Repo locations:

- libFuzzer targets (clang): `src/test/cpp/kstar/fuzz_*.cpp`
- Corpus replay under CTest (coverage + strict determinism): `src/test/cpp/kstar/*_corpus_runner.cpp`
- Build-local regression tests for promoted inputs (embedded seeds + optional `*.bin` replay):
  - `kstar.energy_matrix_loader_regression`
  - `kstar.conf_search_astar_regression`
  - `kstar.partition_function_regression`

Local sanitizer gate (clang ASan/UBSan):

- `./scripts/kstar_sanitize_gate.sh`

Rule:

- Use `-fsanitize=address,undefined`. Do **not** add `-fsanitize=leak` on top of ASan. Control leak detection via `ASAN_OPTIONS=detect_leaks=1|0`.
- Build dir for sanitizers should live on the Linux filesystem (e.g. `$HOME/kstar-sanitize`), not under `/mnt/c/...`, to avoid WSL/DRVFS startup/discovery crashes.
- On WSL, avoid driving sanitizer builds via `ctest` because GTest discovery runs binaries during test enumeration. Use `kstar_sanitize_gate.sh`, which runs a curated set of binaries directly.
- On WSL, run UBSan-only by default (reliable). Enable ASan+UBSan explicitly:
  - `KSTAR_SANITIZE_ASAN=1 ./scripts/kstar_sanitize_gate.sh`

#### 1) Oracle tests (exact enumeration on tiny spaces)

Goal: catch algorithmic mistakes with ground truth.

Typical oracles:

- Exhaustive enumeration on tiny synthetic `EnergyMatrix` instances.
- Closed-form cases (factorization when pairwise terms are zero).

Repo location:

- `src/test/cpp/kstar/test_partition_function_precision_tier0_gtest.cpp`
- `src/test/cpp/kstar/test_conf_search_astar_synthesized_gtest.cpp` (tiny-space oracle vs `EnergyMatrix::computeEnergy()`)

#### 2) Metamorphic / invariant tests (no oracle required)

Goal: catch correctness regressions without relying on fixed expected outputs.

High-ROI invariants for partition functions:

- Const-term (global energy) shift invariance (log-space shift by known constant).
- Epsilon monotonicity (tighter epsilon must not loosen bounds).
- Determinism (same input → same outputs).
- Baseline vs Fast equivalence (within tolerance).

Repo location:

- `src/test/cpp/kstar/test_partition_function_precision_tier1_gtest.cpp`
- `src/test/cpp/kstar/test_conf_search_astar_synthesized_gtest.cpp` (metamorphic invariants on tiny spaces)

#### 3) Differential tests (two implementations must agree)

Goal: detect regressions by comparing independent or semi-independent implementations.

Examples:

- A* Baseline vs A* Fast partition function bounds.
- Double vs float on tiny spaces (tolerance-based).
- A* vs exact enumeration on tiny spaces (oracle) under epsilon=0.

Repo location:

- Tier0/Tier1 precision tests (above) and synthesized A* equivalence tests.

#### 4) Verbatim contract tests (Java parity)

Goal: lock down expected behaviors on Java-exported inputs (external contract).

Repo location:

- `src/test/cpp/kstar/test_partition_function_gtest.cpp`

#### 5) Formal / bounded checks (small-core math)

Goal: prove specific branch/math properties in small bounded models.

Repo location:

- `src/test/cpp/kstar/cbmc_partition_function_tier2.cpp`
- `src/test/cpp/kstar/cbmc_log_space_tier2.cpp`

### “Missing but high ROI” additions checklist

These are common gaps once the Tier0/Tier1 suite exists.

Partition function:

- [x] Permutation invariance: permuting positions (and remapping indices consistently) must not change results.
- [x] Pairwise-zero factorization: closed-form oracle when pairwise terms are zero.
- [x] Monotonicity: lowering any energy term must not decrease $Z$ (or $\log Z$).
- [x] A* bounds contain exact under epsilon>0 (oracle via epsilon=0, A* run with exact enumeration disabled).
- [x] NaN/Inf policy tests: define and enforce behavior on non-finite energies.

Conf search / A* search:

- [x] Determinism under fixed input (guardrail; also enforced by fuzz corpus runner in strict mode).
- [x] Tie-heavy scenarios to stress ordering and priority-queue behavior (fuzz modes).
- [x] Tiny-space oracle vs exact `EnergyMatrix::computeEnergy()` (baseline + fast).
- [x] Metamorphic: permutation invariance (score multiset).
- [x] Metamorphic: const-term shift invariance (all scores shift by constant).
- [x] Metamorphic: monotonicity under energy lowering (scores non-increasing as a multiset).

### How to run correctness suites (CTest)

Precision tiers:

- Tier 0: `ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.partition_function_precision_tier0`
- Tier 1: `ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.partition_function_precision_tier1`

All precision:

- `ctest --test-dir build/cpp/kstar-coverage --output-on-failure -L precision`

Verbatim partition function:

- `ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.partition_function`

