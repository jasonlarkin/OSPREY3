# Testing Strategy for K* Implementation

## Current Test Status

### Unit Tests (Implemented)
This file is partially historical. The current, canonical correctness mapping is:
- `src/main/cpp/kstar/CORRECTNESS_AND_CXX20_RATIONALE.md`

Key implemented tests live under `src/test/cpp/kstar/` and include:
- A* search unit tests (baseline + fast): `test_astar_search.cpp`
- Partition function contract tests (VERBATIM / Java parity): `test_partition_function_gtest.cpp`
- Partition function equivalence tests (SYNTHESIZED; baseline vs fast): `test_partition_function_synthesized_gtest.cpp`
- Precision tests (SYNTHESIZED; tiered):
  - PartitionFunction Tier0 (oracle on tiny synthetic spaces): `test_partition_function_precision_tier0_gtest.cpp`
  - PartitionFunction Tier1 (metamorphic/invariants): `test_partition_function_precision_tier1_gtest.cpp`
  - Log-space Tier1 (metamorphic/invariants): `test_log_space_precision_tier1_gtest.cpp`
- Additional unit tests for EnergyMatrix, loaders, rankers, etc.

### Missing Tests
The remaining major gaps are:
- **ConfSpace/JNA pipeline integration** (Phase‑2+)
- **End-to-end KStarParallel correctness vs Java `TestKStar`** (Phase‑2+)

## Test Data Sources

### 1. Small Test Cases (`src/test/resources/confSpaces/`)

**Minimal cases** (for unit tests):
- `dipeptide.5hydrophobic.ccsx` - Smallest test case
- `6ov7.tiny.*.ccsx` - Tiny protein (protein, ligand, complex)
  - Good for: Fast unit tests, ConfSpace structure validation

**Small cases** (for integration tests):
- `6ov7.small.*.ccsx` - Small protein
- `6ov7.medium.*.ccsx` - Medium protein
  - Good for: Integration tests, performance profiling

### 2. Real CCSX examples (Phase‑2+)

These are useful once ConfSpace integration exists, but they are not part of Phase‑1 correctness claims.

## Test Categories

### Category 1: ConfSpace Integration Tests

**Purpose**: Verify C++ can access ConfSpace data

**Tests needed**:
1. Load small .ccsx file (dipeptide.5hydrophobic.ccsx)
2. Verify position count
3. Verify conformation count per position
4. Verify atom coordinates access
5. Verify forcefield parameters access

**Source**: `TestKStar.java` shows how to load and validate

### Category 2: PartitionFunction Tests

**Purpose**: Verify partition function computation

**Implemented tests**:
- **VERBATIM** Java-parity bound/convergence checks:
  - `src/test/cpp/kstar/test_partition_function_gtest.cpp`
- **SYNTHESIZED** baseline-vs-fast equivalence:
  - `src/test/cpp/kstar/test_partition_function_synthesized_gtest.cpp`
- **SYNTHESIZED precision tiers** (documented separately):
  - `src/main/cpp/kstar/PRECISION_TESTING.md`

**Expected values**: From `TestKStar.java` test2RL0() - has exact expected partition function values

### Category 3: KStarParallel Tests

**Purpose**: Verify parallel sequence processing

**Tests needed**:
1. Process 2 sequences in parallel
2. Verify results match sequential processing
3. Verify thread safety (no race conditions)
4. Verify result ordering preserved

### Category 4: Correctness Tests

**Purpose**: Validate against Java implementation

**Remaining work**:
- Full confspace-backed K* score end-to-end equivalence with Java’s `TestKStar.java` requires Phase-2+ plumbing (ConfSpace/JNA and energy pipeline integration).

**Test cases from Java**:
- `test2RL0()` - 21 sequences with exact expected K* scores
- Can use these as ground truth

## Test Implementation Plan

### Phase 1: ConfSpace Integration (Next)

**Test file**: `test_confspace_integration.cpp`

**Tests**:
```cpp
void test_load_confspace() {
    // Load dipeptide.5hydrophobic.ccsx
    // Verify structure
}

void test_confspace_access() {
    // Access positions, conformations, atoms
    // Verify data integrity
}
```

**Dependencies**: ConfSpace boundary definition (Phase‑2+)

### Phase 2: PartitionFunction Tests

**Test file**: `test_partition_function.cpp`

**Tests**:
```cpp
void test_partition_function_basic() {
    // Compute partition function
    // Verify convergence
}

void test_partition_function_correctness() {
    // Compare with Java results
    // Use known values from TestKStar.java
}
```

**Dependencies**: ConfSpace integration complete (Phase‑2+)

### Phase 3: KStarParallel Tests

**Test file**: `test_kstar_parallel.cpp`

**Tests**:
```cpp
void test_parallel_sequences() {
    // Process multiple sequences
    // Verify correctness
}

void test_correctness_vs_java() {
    // Load test2RL0 data
    // Compare all 21 sequences
    // Verify K* scores match
}
```

**Dependencies**: ConfSpace + energy pipeline boundary (Phase‑2+)

## Using Java Test Infrastructure

### Test Patterns from `TestKStar.java`

**Loading confspaces**:
```java
ConfSpace complex = ConfSpace.fromBytes(
    FileTools.readResourceBytes("/confSpaces/2RL0.complex.ccsx")
);
```

**Expected values**:
```java
assertSequence(result, 0, "PHE LYS ILE THR PHE ASP GLU",
    "1.599167e+119", "1.603392e+119",  // protein bounds
    "4.697497e+934", "5.044775e+934",  // ligand bounds
    "1.695936e+1117", "1.876755e+1117" // complex bounds
);
```

**C++ equivalent**:
```cpp
// Load same files
// Compute K* scores
// Compare bounds (within epsilon)
```

## Test File Organization

```
Current high-signal files:

src/test/cpp/kstar/
├── test_astar_search.cpp                 # A* (baseline + fast) correctness/equivalence
├── test_partition_function_gtest.cpp     # pfunc VERBATIM contracts (Java parity)
├── test_partition_function_synthesized_gtest.cpp         # pfunc SYNTHESIZED equivalence (baseline vs fast)
├── test_partition_function_precision_tier0_gtest.cpp     # pfunc SYNTHESIZED precision Tier0 (oracle)
├── test_partition_function_precision_tier1_gtest.cpp     # pfunc SYNTHESIZED precision Tier1 (metamorphic/invariants)
├── test_log_space_gtest.cpp                              # log-space SYNTHESIZED basics
├── test_log_space_precision_tier1_gtest.cpp              # log-space SYNTHESIZED precision Tier1 (metamorphic/invariants)
└── (additional unit tests)               # EnergyMatrix, loaders, rankers, etc.
```

## Test categories as CTest labels (for analysis)

This repo uses CTest labels to keep categories clean:

- VERBATIM: `kstar;gtest;verbatim;...`
- SYNTHESIZED (non-precision): `kstar;gtest;synthesized;...`
- SYNTHESIZED precision tiers: `kstar;gtest;synthesized;precision;tier0|tier1;...`
- SYNTHESIZED sanitizers: `kstar;gtest;synthesized;sanitizers`

Convenience entrypoints:

- `kstar.precision_all` (labels: `kstar;gtest;synthesized;precision`)
- `kstar.sanitizers_smoke` (labels: `kstar;gtest;synthesized;sanitizers`; only present when built with `KSTAR_ENABLE_SANITIZERS=ON`)
- See `src/main/cpp/kstar/PRECISION_TESTING.md` for run commands and rationale.
- See `src/main/cpp/kstar/SANITIZER_TESTING.md` for sanitizer build/run commands.

## Next Immediate Steps

1. Keep Phase‑1 correctness tight (EnergyMatrix-only).
2. Expand precision tiers as per `PRECISION_TESTING.md`.
3. Define the Phase‑2 ConfSpace boundary before adding end-to-end K* parity claims.

