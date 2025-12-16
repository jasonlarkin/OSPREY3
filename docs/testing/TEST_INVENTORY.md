# OSPREY Test Inventory

This document provides a comprehensive inventory of all tests defined in the OSPREY codebase, organized by category.

## Overview

OSPREY uses multiple testing approaches:
- **Examples**: High-level example configurations and scripts
- **Java Tests**: JUnit tests in `src/test/java`
- **C++ Tests**: Google Test (gtest) tests in `src/main/cc`
- **Python Examples**: Tested via `TestPythonScripts.java`

---

## 1. Examples Directory (`examples/`)

Examples are organized by functionality and serve as integration tests/documentation:

### Configuration-Based Examples

#### 1CC8 Examples (Multiple Variants)
- `1CC8/` - Main 1CC8 example
  - System configuration, DEE, KStar configs
  - Test results directory
- `1CC8.13/` - 1CC8 variant with 13 rotamers
- `1CC8.deeper/` - Deeper search variant
- `1CC8.junit/` - Variant for JUnit testing

#### Other Protein Examples
- `1FSV/` - 1FSV protein example
- `1GUA/` - 1GUA example (minimization)
- `2O9S_L2/` - 2O9S ligand example
- `2RL0.kstar/` - 2RL0 KStar example
- `3K75.3LQC/` - Multistate design example
- `4HEM/` - 4HEM example
- `4NPD/` - 4NPD example
- `adi.1AHO/` - 1AHO example
- `DAGK/` - DAGK example
- `comets.junit/` - COMETS multistate example for JUnit

### Python Script Examples

#### GMEC (Global Minimum Energy Conformation) Examples
Location: `examples/python.GMEC/`
- `analyzeConf.py`
- `findGMEC.py` (basic)
- `findGMEC.advanced.py`
- `findGMEC.DEE.py`
- `findGMEC.DEEPer.py`
- `findGMEC.externalMemory.py`
- `findGMEC.energyPartitions.py`
- `findGMEC.LUTE.train.py` & `findGMEC.LUTE.design.py`
- `findGMEC.multipleStrands.py`
- `findGMEC.referenceEnergies.py`
- `findGMEC.resEntropy.py`
- `findGMEC.confDB.py`
- `findGMEC.cats.py`
- `templateLibrary.py`
- `wildtypeEnergies.py`
- `plug.py`
- `comets.py`
- `comets.boundedMemory.py`

#### KStar Examples
Location: `examples/python.KStar/`
- `kstar.py` (basic)
- `bbkstar.py` (Branch-and-Bound KStar)
- `markstar.kstar.py`
- `markstar.bbkstar.py`
- `LUTE.train.py`
- `LUTE.kstar.py`
- `LUTE.bbkstar.py`
- `LUTE.confSpaces.py`

#### Other Python Examples
- `examples/python.SOFEA/` - SOFEA free energy analysis
  - `sofea.py`
  - `residueEntropy.py`
  - `residueEntropy.multi-state.py`
  - `freeEnergy.py`
- `examples/python.PAStE/` - PAStE (Partitioning and Search for Top-ranked Ensembles)
  - `paste.py`
  - Multiple PDB output directories
- `examples/python.EWAKStar/` - EWAKStar examples
  - `ewakstar.py`
  - `ewakstar.2hnv.py`
  - Subdirectories: `1A0R/`, `1GWC/`
- `examples/python.ccs/` - Continuous Conformational Space (CCS)
  - `conf space prep/prep.py`
  - `kstar/kstar.py`, `kstar.boundedMem.py`, `kstar.slurm.py`
  - `molecule prep/prep.py`
- `examples/ccs.D-peptide-L-protein/` - D-peptide/L-protein complex
  - Multiple preprocessing and submission scripts
- `examples/gpu/` - GPU diagnostics
  - `diagnostics.py`

---

## 2. Java Tests (`src/test/java/`)

Java tests use JUnit 5 (Jupiter). Tests are organized by package following the main source structure.

### Test Framework
- **Framework**: JUnit 5 (Jupiter)
- **Build Tool**: Gradle
- **Total Test Files**: ~150+ test files

### Test Categories

#### Core Algorithms & Search

**A* Search (`astar/`)**
- `TestConfIndex.java`
- `TestConfRanker.java`
- `TestConfSearchCache.java`
- `TestLinkedConfAStarNode.java`
- `TestSequencePruner.java`
- `TestSMAStar.java`
- `BenchmarkAStar.java`
- `BenchmarkConfRanker.java`
- `SMAStarLab.java`
- `Matchers.java`

**Conformation Space (`confspace/`)**
- `TestConf.java`
- `TestConfDB.java`
- `TestConfSearchMultiSplitter.java`
- `TestConfSearchSplitter.java`
- `TestRCIndexMap.java`
- `TestSequence.java`
- `TestSimpleConfSpace.java`
- `TestTupleTree.java`
- Compiled tests: 5 files in `compiled/`

**Top-level Search Tests**
- `TestAStar.java`
- `TestConfSearch.java`
- `TestConfSpace.java`
- `ConfSpaceTests.java`

#### Energy Calculations

**Energy Matrix (`ematrix/`)**
- `TestEmatCorrections.java`
- `TestEnergyPartitions.java`
- `TestSimpleEnergyCalculator.java`
- `TestSimpleEnergyMatrixCalculator.java`
- `TestSimplerEnergyMatrixCalculator.java`
- `BenchmarkEmat.java`
- `EnergyPartitionsPlayground.java`
- Compiled tests: 1 file in `compiled/`

**Energy Functions (`energy/`)**
- `TestMinimizingEnergyCalculators.java`
- `TestResidueForcefieldBreakdown.java`
- `TestRigidEMatVsNBodyMinimization.java`
- `TestUpdatingEnergyMatrix.java`
- `SmallMoleculeEnergyTest.java`
- `MisBoundPlayground.java`
- Forcefield tests: 7 files in `forcefield/`
- Compiled tests: 3 files in `compiled/`

**Top-level Energy Tests**
- `TestAmberEnergy.java`
- `BenchmarkEnergies.java`
- `EnergyMatrixProfiling.java`
- `EnergyProfiling.java`

#### GMEC (Global Minimum Energy Conformation)

**Location**: `gmec/`
- `TestFindGMEC.java`
- `TestSimpleGMECFinder.java`
- `TestDEEGMECFinder.java`
- `TestComets.java`
- `BenchmarkGMECFinder.java`
- `CometsLab.java`

**Top-level**
- `TestCOMETS.java`

#### KStar

**Location**: `kstar/`
- `TestKStar.java`
- `TestBBKStar.java`
- `TestKStarScore.java`
- `TestMSKStar.java` (Multi-State KStar)
- `TestSequenceAnalyzer.java`
- `TestSimplePartitionFunction.java`
- `BenchmarkPartitionFunction.java`
- `KStarRunner.java`
- `NewMethPlayground.java`
- `PfuncPlayground.java`
- Compiled tests: 3 files in `compiled/`
  - `TestKStar.java`
  - `TestBounds.java`
  - `TestBBKStar.java`

#### MARKStar

**Location**: `markstar/`
- `TestMARKStar.java` (~1000+ lines, comprehensive)
- `TestMARKStarDesignProblems.java` (~750+ lines)

#### LUTE (Lower Upper Tuple Enumeration)

**Location**: `lute/`
- `TestLute.java`
- `TestLUTEIO.java`
- `TestRandomizedDFSSampler.java`
- `BenchmarkTupleIndex.java`
- `LUTELab.java`

#### Minimization

**Location**: `minimization/`
- `TestMinimization.java` (extensive, 25+ test methods)
- `TestMinimizationStability.java`
- `TestAmberWells.java`
- `BenchmarkMinimization.java`
- `ExampleParallelMinimization.java`
- `MinimizationComparison.java`

#### Pruning

**Location**: `pruning/`
- `TestSimpleDEE.java` (Dead-End Elimination)
- `TestTransitivePruning.java`
- `TestUnprunedConfBounding.java`
- `TestPLUG.java`
- Plus 2 more files

#### Structure & Molecular Modeling

**Location**: `structure/`
- `TestAtom.java`
- `TestAtomConnectivity.java` (extensive, 15+ test methods)
- `TestPDBIO.java`
- `TestOMOLIO.java`
- `TestProbe.java`
- `TestStrand.java`

#### Degrees of Freedom

**Location**: `dof/`
- `TestDOFs.java`
- `TestFreeDihedral.java`

#### Parallelism & Concurrency

**Location**: `parallelism/`
- `TestThreadPoolTaskExecutor.java`
- `TestGenerator.java`
- `BenchmarkMulticore.java`
- `BenchmarkTaskExecutor.java`
- `ForkCluster.java`

#### Partitioning & Clustering

**Location**: `partcr/`
- `TestPartCR.java`
- `TestRCSplits.java`

**Location**: `coffee/`
- `TestCoffee.java`
- `TestClusterZMatrix.java`
- `TestLeafNodeBounds.java`
- `BenchmarkCoffee.java`
- `CoffeeLab.java`
- `DebugSerializers.java`
- NodeDB tests: 3 files
- SeqDB tests: 1 file

#### SOFEA (Sequence Optimization with Free Energy Analysis)

**Location**: `sofea/`
- `TestSofea.java` (extensive, 50+ test methods)
- `TestSeqDB.java`
- `TestFringeDB.java`
- `TestBigDecimalIO.java`
- Plus 3 more files

#### External Memory & Serialization

**Location**: `externalMemory/`
- `TestAStarSerialization.java`

#### GPU Computing

**Location**: `gpu/`
- `TestEnergyFunctionGenerator.java`
- `BenchmarkForcefieldKernel.java`
- OpenCL tests: 1 file in `opencl/` (`TestGpu.java`)

#### MP/LP (Message Passing / Linear Programming)

**Location**: `mplp/`
- `TestMessageVars.java`
- `MPLPPLayground.java`

#### Design & Commands

**Location**: `design/`
- `MainTest.java`
- `AffinityDesignYAMLParserTest.java`
- Command tests: 2 files in `commands/`

#### Python Script Integration

**Location**: `python/`
- `TestPythonScripts.java` - Tests all Python example scripts
  - GMEC scripts: 15 test methods
  - KStar scripts: 5 test methods
  - SOFEA scripts: 1 test method

#### Tools & Utilities

**Location**: `tools/`
- `TestMathTools.java` (extensive, 12+ test methods)
- `TestBigExp.java` (extensive, 15+ test methods)
- `TestDihedrals.java` (extensive, 15+ test methods)
- `TestBigDecimalMathThreadSafety.java`
- `TestExpFunction.java`
- `TestJvmMem.java`
- `TestPrefixTree.java`
- `TestTimeTools.java`
- `TestResultDoc.java`
- `TestGNUPlot.java`
- Deep Copy/Serialization tests (recent additions):
  - `Trace1CC8Execution.java`
  - `Capture1CC8Objects.java`
  - `CaptureTestObjects.java`
  - `TestDeepCopyValidation.java`
  - `CompareDeepCopyImplementations.java`
  - `BenchmarkDeepCopy.java`
  - `ProfileDeepCopyUsage.java`
  - `AnalyzeSerializedStructure.java`

#### PAStE

**Location**: `paste/`
- `PasteRunner.java`

#### Other Labs & Playgrounds
- `ClusterLab.java`
- `FlexLab.java`
- `ConfTreeProfiling.java`
- `PruningProfiling.java`
- `CapturedIOTest.java`
- `CompileIt.java`
- `ConfSpaceTests.java`

#### Top-level Tests
- `TestBase.java` (base class)
- `TestTools.java`
- `TestWorkCrew.java`
- `TestGenerateRotamerLibrary.java`
- `Benchmark.java`

---

## 3. C++ Tests (`src/main/cc/`)

C++ tests use Google Test (gtest) framework. Tests are organized by C++ module.

### Test Framework
- **Framework**: Google Test (gtest/gmock)
- **Build System**: CMake with `FetchContent` for gtest
- **Test Discovery**: CTest integration via `gtest_discover_tests`

### ConfEcalc Module

**Location**: `src/main/cc/ConfEcalc/tests/`

**Build Configuration**: `ConfEcalc/tests/CMakeLists.txt`
- Executable: `confecalc_tests`
- Links: `ConfEcalc` library, `GTest::gtest`, `GTest::gtest_main`

**Unit Tests** (`unit/`):
1. `test_array.cpp`
   - `ArrayTest` fixture
   - 13 test cases covering:
     - Array creation
     - Element access (read/write)
     - Const access
     - Copy operations (`CopyFrom`, `CopyFromWithOffset`)
     - Truncation
     - Copy constructor deletion (RAII)
     - Bounds checking
     - Different types
     - Move semantics (constructor, assignment, self-assignment)

2. `test_confecalc.cpp`
   - `ConfEcalcTest` fixture
   - Version function tests
   - `noexcept` specification tests

**Test Count**: ~15 test cases total

### DeepCopy Module

**Location**: `src/main/cc/DeepCopy/tests/`

**Build Configuration**: `DeepCopy/tests/CMakeLists.txt`
- Executable: `deepcopy_tests`
- Links: `DeepCopy` library, `GTest::gtest`, `GTest::gtest_main`

**Unit Tests** (`unit/`):
1. `test_basic.cpp`
   - `DeepCopy` basic interface tests
   - Null input handling
   - Zero size handling

2. `test_stream_parser.cpp`
   - `StreamParserTest` fixture
   - 11 test cases:
     - Header parsing (valid/invalid magic, version)
     - Stream validation (too short, empty, truncated)
     - Object reading (null, string, reference, basic object)
     - Null pointer handling

3. `test_class_descriptor.cpp`
   - `ClassDescriptorTest` fixture
   - 4 test cases:
     - Minimal class descriptor parsing
     - Class descriptor with fields
     - Object field parsing
     - Class descriptor references

4. `test_field_parsing.cpp`
   - `FieldParsingTest` fixture
   - 6 test cases:
     - Primitive fields (int, multiple primitives)
     - Object fields
     - Null fields
     - Reference fields
     - Nested objects

5. `test_array_parsing.cpp`
   - `ArrayParsingTest` fixture
   - 9 test cases:
     - Primitive arrays (int, double, boolean)
     - Empty arrays
     - String arrays (with nulls, with references)
     - Multi-dimensional arrays
     - Arrays with class descriptor references

6. `test_algorithm.cpp`
   - `AlgorithmTest` fixture
   - 6 test cases:
     - BFS traversal
     - Recursion limit handling
     - Handle table consistency
     - Memory efficiency
     - Error handling (no leaks)
     - Determinism

7. `test_reference_resolution.cpp`
   - `ReferenceResolutionTest` fixture
   - 4 test cases:
     - Valid reference resolution
     - Circular references
     - Self references
     - Multiple references

**Integration Tests** (`integration/`):
1. `test_captured_data.cpp`
   - `IntegrationTest` fixture
   - 6 test cases:
     - Simple object deserialization
     - Nested object deserialization
     - Circular node handling
     - Deep nested structures
     - All captured test data
     - 1CC8 MoleculeModifierAndScorer (real OSPREY object)

2. `test_1cc8_trace.cpp`
   - `OneCC8Trace` test
   - Trace deserialization test (end-to-end validation)

**Test Count**: ~50+ test cases total

---

## 4. Test Statistics Summary

### Examples
- **Total Example Directories**: ~25+
- **Python Scripts**: 52 scripts
- **Configuration Files**: ~50+ `.cfg` files

### Java Tests
- **Total Test Files**: ~150+ files
- **Test Methods**: 1000+ individual `@Test` methods
- **Packages**: 30+ test packages
- **Notable Large Test Files**:
  - `TestSofea.java`: 580+ lines, 50+ tests
  - `TestMARKStar.java`: 1000+ lines, 20+ tests
  - `TestMARKStarDesignProblems.java`: 750+ lines, 20+ tests
  - `TestMinimization.java`: 25+ test methods
  - `TestAtomConnectivity.java`: 15+ test methods
  - `TestPythonScripts.java`: 20+ test methods (covering all Python examples)

### C++ Tests
- **Total Test Files**: 11 files
- **Test Cases**: ~65+ individual test cases
- **Modules**: 2 (ConfEcalc, DeepCopy)
- **Unit Tests**: 9 files
- **Integration Tests**: 2 files

---

## 5. Test Execution

### Java Tests (Gradle)
```bash
# Run all tests
./gradlew test

# Run specific test class
./gradlew test --tests "edu.duke.cs.osprey.tools.Trace1CC8Execution"

# Run specific test method
./gradlew test --tests "edu.duke.cs.osprey.tools.Trace1CC8Execution.trace1CC8Capture"

# Compile only (fast iteration)
./gradlew compileJava compileTestJava
```

### C++ Tests (CMake/CTest)
```bash
# Build and run ConfEcalc tests
cd src/main/cc/ConfEcalc
./build-and-test.sh
# or
cd build && ctest

# Build and run DeepCopy tests
cd src/main/cc/DeepCopy
./build-and-test.sh
# or
cd build && ctest

# Run specific test
cd build/tests && ./confecalc_tests --gtest_filter="ArrayTest.*"
```

### Python Examples
```bash
# Python examples are tested via TestPythonScripts.java
./gradlew test --tests "edu.duke.cs.osprey.python.TestPythonScripts"

# Or run manually:
cd examples/python.GMEC
python findGMEC.py
```

---

## 6. Test Coverage Areas

### Functional Coverage

**Core Algorithms**:
- A* search variants (SMA*, ConfRanker)
- ConfSpace enumeration and search
- Energy calculations (rigid, minimizing, partitioned)
- GMEC finding (DEE, Simple, COMETS)
- KStar calculations (BBKStar, MSKStar, MARKStar)
- LUTE tuple enumeration
- Pruning (DEE, Transitive, PLUG)

**Data Structures**:
- ConfDB, Sequence, TupleTree
- Energy matrices
- Node databases (Coffee)
- Sequence databases (SOFEA)

**I/O & Serialization**:
- PDB/OMOL file I/O
- Java serialization/deserialization
- Deep copy validation (Java ↔ C++)
- A* state serialization

**Performance & Optimization**:
- Parallelism (thread pools, task executors)
- GPU acceleration (OpenCL)
- Memory efficiency (external memory, bounded memory)
- Minimization stability

**Molecular Modeling**:
- Atom connectivity
- Degrees of freedom
- Structure manipulation
- Energy forcefields

### Test Types

**Unit Tests**:
- Individual component testing
- Isolated functionality validation
- Fast execution (< 1 second per test)

**Integration Tests**:
- End-to-end workflows
- Real data validation (1CC8, 2RL0, etc.)
- Cross-language validation (Java ↔ C++)
- Python script execution

**Benchmark Tests**:
- Performance profiling
- Memory usage tracking
- Scalability testing

**Example/Regression Tests**:
- Configuration file validation
- Output consistency checks
- Historical result comparison

---

## 7. Notable Test Data

**Test Resources** (`src/test/resources/`):
- Protein structures (1CC8, 2RL0, gp120, etc.)
- Configuration spaces (`.confspace`, `.ccsx` files)
- Serialized test data (`test-data/serialized/`)
- Reference results (`testResults/`)

**Example Test Data** (`examples/`):
- PDB files for various proteins
- Energy matrices (`.EMAT.dat`, `.EPICMAT.dat`)
- Configuration files (`.cfg`)
- Expected outputs (`.GMEC.pdb`, `.confs.txt`)

---

## 8. Test Maintenance Notes

### Recent Additions
- Deep Copy/Serialization tests (tools/ directory)
- C++ deserializer integration tests
- 1CC8 trace execution tests

### Test Organization
- Tests mirror source package structure
- Benchmarks and labs separate from unit tests
- Compiled tests for integration testing
- Python examples tested via Java integration

### Known Test Patterns
- Base class: `TestBase.java`
- Matchers: `astar/Matchers.java`
- Test data loading utilities in tools package
- C++ test fixtures for common setup

---

## 9. Documentation References

Related documentation files:
- `../../FAST_ITERATION_TIPS.md` - Tips for running tests efficiently
- `../cpp/CPP_IMPLEMENTATION_INVENTORY.md` - Details on C++ implementation and tests
- `../../SERIALIZATION_FIXES.md` - Deep copy/serialization testing details

---

*Last Updated: 2025-12-16
*Generated from codebase analysis*

