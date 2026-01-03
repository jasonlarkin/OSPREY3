# Testing Plan (Phase‑1)

This file is intentionally short. The canonical “what is proved by which tests” mapping is:

- `src/main/cpp/kstar/CORRECTNESS_AND_CXX20_RATIONALE.md`

Precision tiers (synthetic, deterministic) are documented in:

- `src/main/cpp/kstar/PRECISION_TESTING.md`

## Run all tests

```bash
ctest --test-dir build/cpp/kstar --output-on-failure
```

## Run targeted sets

```bash
# partition function + verbatim contracts
ctest --test-dir build/cpp/kstar --output-on-failure -R partition_function

# A* tests
ctest --test-dir build/cpp/kstar --output-on-failure -R astar

# precision tiers
ctest --test-dir build/cpp/kstar --output-on-failure -L precision
```

## Phase‑2+ (explicit)

ConfSpace/JNA integration and end-to-end K* score parity vs Java are Phase‑2+ tasks and are not covered by Phase‑1 tests.

