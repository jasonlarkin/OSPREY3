# Precision Testing (SYNTHESIZED): Numeric Stability + Cross-Implementation Guardrails

This document describes the **precision-focused SYNTHESIZED** test category for the C++ K* port.
It is complementary to:

- `CORRECTNESS_AND_CXX20_RATIONALE.md` (high-level correctness index)
- VERBATIM Java-parity tests (contract assertions on Java-exported `*.emat.bin`)

Goals:

- Detect precision regressions early (log-space arithmetic, bounds math, convergence behavior).
- Provide deterministic, reproducible checks that do **not** require Java-exported test data.
- Keep categories clean for analysis via **CTest labels**.

## Test category taxonomy (for analysis)

These tests are always labeled as:

- `kstar;gtest;synthesized;precision;...`

They are intentionally separate from:

- `kstar;gtest;verbatim;...` (OSPREY Java parity)
- `kstar;gtest;synthesized;...` (non-precision synthesized smoke/equivalence)

## How to run (CTest)

Assuming a configured build tree (example uses the coverage build):

```bash
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -L precision
```

## Fast build (only the precision binaries)

In a fresh build tree, `cmake --build` without a target may build every test binary.
To build only the executables used by the precision suite:

```bash
cmake --build build/cpp/kstar-coverage -j --target kstar_precision_build
```

Run a single tier:

```bash
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -L "tier0"
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -L "tier1"
```

Run a single precision entrypoint:

```bash
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.partition_function_precision_tier0
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.partition_function_precision_tier1
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.log_space_precision_tier1
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.precision_all
```

## Tier 0: PartitionFunction precision (oracle-style on tiny synthetic spaces)

### What it proves

Tier 0 focuses on “strong oracles” on **tiny** conf spaces where exact enumeration is cheap.

- **A* (Baseline/Fast) matches exact enumeration** for `lower_bound/upper_bound` on many random tiny `EnergyMatrix` instances (reproducible RNG seed).
- **Float vs double**: exact-enumeration results are close (tolerance-based) on the same tiny spaces.

### Why this is safe as an oracle

The implementation has an explicit exhaustive enumeration path:

- `PartitionFunction<T>::computeExactByEnumeration` in `partition_function.cpp`

The Tier0 test forces A* execution by disabling the shortcut in the A* runs while still using it as the oracle.

### Important note on `num_confs`

Do **not** treat `num_confs` as “must equal total confs” in A* runs.
A* may stop once the **remaining contribution becomes numerically negligible** due to log-space dominance cutoffs.
In that case, `upper_bound` can become numerically equal to `lower_bound` even when unscored conformations remain.

### Current implementation

- GTest file: `src/test/cpp/kstar/test_partition_function_precision_tier0_gtest.cpp`
- CTest entry: `kstar.partition_function_precision_tier0`
- Labels: `kstar;gtest;synthesized;precision;tier0;partition_function`

## Tier 1: Log-space precision (metamorphic/invariant checks)

### What it proves

Tier 1 validates core invariants of log-space operations used throughout partition function bounds math:

- **NaN propagation**
- **Add bounds**:
  - \(log10Add(a,b) \ge \max(a,b)\)
  - \(log10Add(a,b) \le \max(a,b) + \log_{10}(2)\)
- **Sub correctness in safe ranges**: checks against linear-space computation for moderate inputs.
- **Dominance cutoff behavior** for both add and sub.

### Why the cutoff constant is `36`

In `log_space.hpp`, both `log10Add` and `log10Sub` use a “dominance cutoff” at ~36 decades:

- If `a - b > 36`, then \(10^{b-a} \le 10^{-36}\), so the smaller term cannot affect an IEEE-754 `double` result after rounding.
- The threshold is conservative for `double` and also covers `float`.

This is an intentional performance + stability tradeoff: it avoids unnecessary `pow/log10` work when the outcome is unavoidably the larger term.

### Current implementation

- GTest file: `src/test/cpp/kstar/test_log_space_precision_tier1_gtest.cpp`
- CTest entry: `kstar.log_space_precision_tier1`
- Labels: `kstar;gtest;synthesized;precision;tier1;log_space`

## Tier 1: PartitionFunction precision (metamorphic/invariant checks)

Tier 1 expands precision testing beyond the exact-enumeration oracle by asserting invariants that should hold under algorithmic/parameter variation:

- **Epsilon monotonicity**: tightening epsilon should not loosen bounds.
- **Determinism**: same input/options produce the same result.
- **Const-term shift invariance (oracle via exact enum)**: shifting all energies by a constant should shift `log10(Q)` by the analytically expected amount.

Current implementation:

- GTest file: `src/test/cpp/kstar/test_partition_function_precision_tier1_gtest.cpp`
- CTest entry: `kstar.partition_function_precision_tier1`
- Labels: `kstar;gtest;synthesized;precision;tier1;partition_function`

## Relationship to other precision work (future)

- Tier 2: **CBMC-style bounded proofs** for restricted domains (small, pure functions only).
- Higher-precision backends (planned/optional): `PartitionFunctionMPFR` for arbitrary precision comparisons.

### Tier 2 (CBMC): log-space early-return/cutoff proofs

This tier runs CBMC against a small harness that targets only the **early-return** branches in `log_space.hpp`
(so CBMC does not need to reason about `pow/log10` internals).

- Harness: `src/test/cpp/kstar/cbmc_log_space_tier2.cpp`
- CTest entry: `kstar.cbmc_log_space_tier2`
- Labels: `kstar;cbmc;synthesized;precision;tier2;log_space`

Run:

```bash
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.cbmc_log_space_tier2
```

If `cbmc` is not installed, the test **skips**. Install:

```bash
sudo apt-get install -y cbmc
```

### Tier 2 (CBMC): PartitionFunction delta/cutoff invariants

This tier targets the **delta computation** structure used in `partition_function.cpp` (A* and GD paths),
specifically the sentinel/cutoff branches that do not require modeling `pow` precisely.

- Harness: `src/test/cpp/kstar/cbmc_partition_function_tier2.cpp`
- CTest entry: `kstar.cbmc_partition_function_tier2`
- Labels: `kstar;cbmc;synthesized;precision;tier2;partition_function`

Run:

```bash
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.cbmc_partition_function_tier2
```

### Tier 2 (CBMC): single entrypoint

Run all Tier 2 CBMC checks:

```bash
ctest --test-dir build/cpp/kstar-coverage --output-on-failure -R kstar\\.cbmc_all
```
