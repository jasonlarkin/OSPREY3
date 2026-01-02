# EnergyMatrix C++ Implementation Validation

## Overview

The C++ `EnergyMatrix` implementation has been validated against Java OSPREY `EnergyMatrix` across multiple test cases of varying sizes. All tests confirm numerical correctness within floating-point precision.

## Test Cases

| Test Case | Positions | Total One-Body | Total Pairwise | Status |
|-----------|-----------|----------------|----------------|--------|
| `dipeptide.5hydrophobic` | 2 | 34 | 289 | PASSED |
| `6ov7.tiny.complex` | 3 | - | - | PASSED |
| `2RL0.A` | 4 | - | - | PASSED |
| `1dg9.6f.complex` | 5 | - | - | PASSED |
| `6ov7.small.complex` | 6 | - | - | PASSED |
| `2RL0.complex` | 7 | - | - | PASSED |

## Test Results

All test cases passed with energy calculations matching Java results:

```
=== EnergyMatrix Java Comparison Tests ===

Loading Java EnergyMatrix: test_data/dipeptide.5hydrophobic.emat.bin
  Loaded: 2 positions
  Test conf: [0, 0]
  Expected energy: -1.51448
  Computed energy: -1.51448
  Difference: 2.22045e-16 (rel: 1.46614e-16)
  PASSED

Loading Java EnergyMatrix: test_data/6ov7.tiny.complex.emat.bin
  Loaded: 3 positions
  Test conf: [0, 0, 0]
  Expected energy: 227866
  Computed energy: 227866
  Difference: 0 (rel: 0)
  PASSED

Loading Java EnergyMatrix: test_data/2RL0.A.emat.bin
  Loaded: 4 positions
  Test conf: [0, 0, 0, 0]
  Expected energy: -1098.18
  Computed energy: -1098.18
  Difference: 4.54747e-13 (rel: 4.14093e-16)
  PASSED

Loading Java EnergyMatrix: test_data/6ov7.small.complex.emat.bin
  Loaded: 6 positions
  Test conf: [0, 0, 0, 0, 0, 0]
  Expected energy: 222313
  Computed energy: 222313
  Difference: 0 (rel: 0)
  PASSED

Loading Java EnergyMatrix: test_data/1dg9.6f.complex.emat.bin
  Loaded: 5 positions
  Test conf: [0, 0, 0, 0, 0]
  Expected energy: 35664
  Computed energy: 35664
  Difference: 0 (rel: 0)
  PASSED

Loading Java EnergyMatrix: test_data/2RL0.complex.emat.bin
  Loaded: 7 positions
  Test conf: [0, 0, 0, 0, 0, 0, 0]
  Expected energy: -1323.99
  Computed energy: -1323.99
  Difference: 4.54747e-13 (rel: 3.43467e-16)
  PASSED

=== All comparison tests passed ===
```

## Implementation Details

### Byte Order Conversion

Java `DataOutputStream` writes in **big-endian** (network byte order), while C++ reads in native byte order (little-endian on x86_64). The implementation includes conversion functions:

- `readBigEndianInt32()` - Converts 32-bit integers using `ntohl()`
- `readBigEndianDouble()` - Converts 64-bit doubles using manual byte swapping

### File Format

Binary format written by Java:
1. `constTerm` (double, 8 bytes, big-endian)
2. `numPositions` (int32, 4 bytes, big-endian)
3. `numConfsPerPos` (int32[numPositions], big-endian)
4. `oneBody` array (double[], big-endian)
5. `pairwise` array (double[], big-endian)

### Energy Calculation

The C++ `EnergyMatrix::computeEnergy()` matches Java `EnergyMatrix.confE()`:

```cpp
T energy = const_term_;
// Sum one-body energies
for (int32_t pos = 0; pos < num_positions_; ++pos) {
    energy += getOneBody(pos, conf[pos]);
}
// Sum pairwise energies (only for pos1 > pos2)
for (int32_t pos1 = 1; pos1 < num_positions_; ++pos1) {
    for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
        energy += getPairwise(pos1, conf[pos1], pos2, conf[pos2]);
    }
}
```

## Test Infrastructure

### Java Export Test

`ExportEnergyMatrixForCppTest.java` exports EnergyMatrix data from Java:
- Loads `.ccsx` (compiled conf space) files
- Computes energy matrix using `EmatCalculator`
- Writes binary format for C++ consumption
- Exports test conformation and expected energy

### C++ Comparison Test

`test_energy_matrix_java_comparison.cpp` validates C++ implementation:
- Loads binary files exported from Java
- Computes energy for test conformation
- Compares with expected energy from Java
- Validates within floating-point tolerance (1e-6)

## Running Tests

### Export from Java

```bash
./gradlew test --tests "edu.duke.cs.osprey.kstar.ExportEnergyMatrixForCppTest" --no-daemon
```

### Run C++ Comparison

```bash
cd build/cpp/kstar
make energy_matrix_java_test
./energy_matrix_java_test
```

## Next Steps

With EnergyMatrix validated, the next implementation phases are:

1. **ConfSpace Integration** - Load `.ccsx` files and extract conformation spaces
2. **A* Search Implementation** - Implement A* tree traversal for partition function
3. **Partition Function** - Complete `PartitionFunction::compute()` implementation
4. **K* Parallel Processing** - Integrate with `KStarParallel` for sequence-level parallelism

## Files

- **C++ Implementation**: `src/main/cpp/kstar/energy_matrix.{hpp,cpp}`
- **C++ Loader**: `src/main/cpp/kstar/energy_matrix_loader.{hpp,cpp}`
- **Java Export**: `src/test/java/edu/duke/cs/osprey/kstar/ExportEnergyMatrixForCppTest.java`
- **C++ Test**: `src/test/cpp/kstar/test_energy_matrix_java_comparison.cpp`

