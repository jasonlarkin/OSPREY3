# Partition Function Debug Analysis

This file is a historical debugging log for early Phase‑1 failures. The current implementation has moved past these issues.

## Update (2025-12-29): EnergyMatrix sanity + test-data mismatch resolved

### Key finding: earlier “verbatim” failures were driven by mismatched EnergyMatrix inputs

The original C++ “verbatim” `TestSimplePartitionFunction` ports were **not actually using the same ConfSpace**
as the Java tests. They were loading CCSX/compiled ConfSpace exports (eg `2RL0.G.emat.bin`) while comparing
against `TestSimplePartitionFunction` expected Q* values that come from **programmatically-constructed**
`SimpleConfSpace` objects.

We added new Java exporters that build the **exact same** `SimpleConfSpace` as `TestSimplePartitionFunction`
and export matching EnergyMatrices:

- `2RL0.TestSimplePartitionFunction.protein.emat.bin`
- `2RL0.TestSimplePartitionFunction.ligand.emat.bin`
- `2RL0.TestSimplePartitionFunction.complex.emat.bin`
- `2RL0.NoPositions.protein.emat.bin` (0 positions)

### EnergyMatrix correctness (Java vs C++)

`energy_matrix_java_test` was run against the newly exported matrices and showed **exact match** for the
exported “all-zero RC” conformation energies (expected_energy == computed_energy) for:

- dipeptide
- 6ov7 tiny/small
- 2RL0 NoPositions (0 positions; parsing of the empty conformation string needs a small harness tweak)
- 2RL0 TestSimplePartitionFunction protein/ligand/complex
- 1dg9

This strongly indicates the binary export/import format and `EnergyMatrix::computeEnergy()` logic are correct.

### Partition function status after fixing test-data mismatch

- **2RL0 Protein / Ligand**: now pass the OSPREY bound checks when we compute Q* exactly by enumeration
  (small spaces).
- **2RL0 Complex**: conformation space is ~1.09e8 conformations, so we use the A*-based approximation path.

## Update (2025-12-29): Root cause for complex failure (A* ordering bug) fixed

### Symptom
For 2RL0 Complex, C++ was exploring extremely high-energy conformations first, leading to
Boltzmann weights that underflowed to 0 and a computed `Q* = 0`, causing the verbatim bound check to fail.

### Root cause
The open-set ordering for A* nodes was wrong due to relying on overloaded `operator>` + `std::greater`.
This produced the wrong heap ordering (not guaranteed to pop the minimum f=g+h node).

### Fix
Replace the priority queue comparator with an explicit comparator that makes `top()` the node with
minimum \(f=g+h\), and remove the overloaded `operator>` from `AStarNode`.

### Result
After the fix, `partition_function_test` now passes for:
- 2RL0 Protein (exact enumeration path)
- 2RL0 Ligand (exact enumeration path)
- 2RL0 Complex (A* approximation path)
- 2RL0 NoPositions Protein (exact enumeration path)

## Current debugging entrypoints (if a regression happens)

Use these instead of re-opening the old hypotheses above:

- Run all tests:
  - `ctest --test-dir build/cpp/kstar --output-on-failure`
- Narrow to partition function:
  - `ctest --test-dir build/cpp/kstar --output-on-failure -R partition_function`
- Precision oracle tiers:
  - see `src/main/cpp/kstar/PRECISION_TESTING.md`

