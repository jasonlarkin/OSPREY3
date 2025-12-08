+++
menuTitle = "Deep Copy Testing Strategy"
title = "C++ Deep Copy Testing Strategy"
weight = 100
+++

# Task 2: Comprehensive Testing Strategy

## Testing Hierarchy

### Level 1: Unit Tests (C++ Google Test)
**Scope:** Individual components in isolation
**Location:** `src/main/cc/DeepCopy/tests/unit/`
**Purpose:** Validate correctness of core algorithms and data structures

**Test Categories:**
1. **Stream Parsing Tests** (`test_stream_parser.cpp`) - DONE
   - Parse Java serialization header (magic, version)
   - Read primitive types (byte, short, int, long)
   - Read strings (TC_STRING, TC_LONGSTRING)
   - Read handles (TC_REFERENCE)
   - Handle invalid/malformed streams
   - Edge cases: empty streams, truncated data

2. **Class Descriptor Tests** (`test_class_descriptor.cpp`) - DONE
   - Parse class descriptors (TC_CLASSDESC)
   - Handle field descriptors (primitives, objects, arrays)
   - Handle class descriptor references (TC_REFERENCE)
   - Parse super class descriptors

3. **Field Parsing Tests** (`test_field_parsing.cpp`) - DONE
   - Parse primitive fields (int, double, boolean, etc.)
   - Parse object reference fields
   - Parse null fields
   - Parse nested object structures

4. **Array Parsing Tests** (`test_array_parsing.cpp`) - DONE
   - Parse primitive arrays (int[], double[], boolean[])
   - Parse object arrays (String[])
   - Parse multi-dimensional arrays (int[][])
   - Parse empty arrays
   - Parse arrays with null elements
   - Parse arrays with references

5. **Reference Resolution Tests** (`test_reference_resolution.cpp`) - DONE
   - Resolve forward references
   - Handle circular references
   - Handle self-references
   - Handle multiple references to same object

6. **Algorithm Tests** (`test_algorithm.cpp`) - DONE
   - BFS traversal correctness
   - Work queue management
   - Iterative vs recursive (depth limits)
   - Memory efficiency

### Level 2: Integration Tests (C++ with Java-Serialized Data)
**Scope:** C++ deserializer with real Java-serialized objects
**Location:** `src/main/cc/DeepCopy/tests/integration/`
**Purpose:** Validate C++ can parse actual Java serialization format

**Test Categories:**
1. **Java Serialization Format Tests** (`test_java_format.cpp`)
   - Use Java to serialize test objects
   - Pass serialized bytes to C++ deserializer
   - Verify C++ reconstructs object correctly
   - Test with OSPREY's test object classes

2. **OSPREY Object Tests** (`test_osprey_objects.cpp`)
   - Serialize OSPREY objects (SimpleObject, NestedObject, CircularNode from BenchmarkDeepCopy)
   - Deserialize in C++
   - Compare structure and values
   - Derived from `BenchmarkDeepCopy.java` test classes

3. **Reference Resolution Tests** (`test_references.cpp`)
   - Circular references
   - Multiple references to same object
   - Null references
   - Deep nesting

### Level 3: System Tests (Java + C++ via JNA)
**Scope:** Full integration with OSPREY codebase
**Location:** `src/test/java/edu/duke/cs/osprey/tools/`
**Purpose:** Validate C++ deep copy works in production OSPREY code

**Test Categories:**
1. **Direct Replacement Tests** (`TestDeepCopyNative.java`)
   - Replace `ObjectIO.deepCopy()` with `DeepCopyNative.deepCopy()`
   - Test with same objects as `BenchmarkDeepCopy`
   - Verify identical results
   - Compare performance

2. **OSPREY Integration Tests** (`TestDeepCopyIntegration.java`)
   - Test with actual OSPREY objects:
     - `MoleculeModifierAndScorer` (from SAPE)
     - `EPICMatrix` (from VoxelGCalculator)
     - `ResidueTemplate` (from DAminoAcidHandler)
     - `KSSearchProblem` (from PFParallel2)
   - Verify correctness for each type
   - Measure performance improvements

3. **Ground Truth Tests** (`TestDeepCopyCorrectness.java`)
   - Use `TestFindGMEC.test1CC8()` as ground truth
   - Run with Java deepCopy → capture results
   - Run with C++ deepCopy → compare results
   - Must produce identical GMEC energy and assignments
   - Derived from: `TestFindGMEC.java`

### Level 4: Performance & Stress Tests
**Scope:** Performance validation and edge cases
**Location:** Mixed (C++ unit + Java integration)
**Purpose:** Validate performance goals and handle extreme cases

**Test Categories:**
1. **Performance Benchmarks** (`test_performance.cpp`, `BenchmarkDeepCopyNative.java`)
   - Compare Java vs C++ performance
   - Measure throughput (objects/sec)
   - Memory usage comparison
   - Derived from: `BenchmarkDeepCopy.java`

2. **Stress Tests** (`test_stress.cpp`)
   - Very deep nesting (10,000+ levels)
   - Large object graphs (millions of objects)
   - Complex circular references
   - Memory pressure scenarios

3. **Correctness Under Load** (`TestDeepCopyStress.java`)
   - Run `TestFindGMEC` with C++ deep copy
   - Verify no stack overflow (unlimited depth)
   - Verify correctness maintained
   - Measure performance at scale

## Test Derivation Strategy

### From OSPREY Tests

1. **BenchmarkDeepCopy.java → C++ Unit Tests**
   - `SimpleObject` → `test_stream_parser.cpp` (simple object parsing)
   - `NestedObject` → `test_nested.cpp` (nested structure)
   - `CircularNode` → `test_circular.cpp` (circular references)
   - Performance metrics → `test_performance.cpp`

2. **TestFindGMEC.java → System Tests**
   - `test1CC8()` → `TestDeepCopyCorrectness.test1CC8WithCpp()`
   - Expected results (energy, assignments) → assertions
   - Performance baseline → comparison target

3. **Production Code Usage → Integration Tests**
   - `SAPE.java:93` → `TestDeepCopyIntegration.testMoleculeModifierAndScorer()`
   - `VoxelGCalculator.java:66` → `TestDeepCopyIntegration.testEPICMatrix()`
   - Other usage sites → individual test cases

### Algorithm-Based Tests

When OSPREY doesn't provide test cases, derive from algorithm properties:

1. **BFS Traversal Properties**
   - All nodes visited exactly once
   - No cycles in traversal (work queue prevents recursion)
   - Order independence (BFS vs DFS shouldn't matter for correctness)

2. **Memory Management Properties**
   - No memory leaks (smart pointers)
   - No double-free
   - Proper cleanup on exceptions

3. **Reference Resolution Properties**
   - Circular references preserved
   - Multiple references resolved correctly
   - Handle table consistency

4. **Edge Cases**
   - Empty object graphs
   - Single object
   - Null references
   - Truncated streams
   - Invalid stream format

## Test Implementation Plan

### Phase 1: Unit Tests (Current Stage)
**Status:** In Progress
**Focus:** Stream parsing and basic deserialization

**Tests Implemented:**
1. Basic interface tests (`test_basic.cpp`) - DONE
2. Stream parser tests (`test_stream_parser.cpp`) - DONE
3. Class descriptor tests (`test_class_descriptor.cpp`) - DONE
4. Field parsing tests (`test_field_parsing.cpp`) - DONE
5. Array parsing tests (`test_array_parsing.cpp`) - DONE
6. Reference resolution tests (`test_reference_resolution.cpp`) - DONE
7. Algorithm correctness tests (`test_algorithm.cpp`) - DONE

**Total: 43 unit tests passing**

### Phase 2: Integration Tests (After Core Implementation)
**Status:** Planned
**Focus:** Java-serialized data parsing

**Tests to Implement:**
1. Java serialization format validation
2. OSPREY object deserialization
3. Reference resolution validation

### Phase 3: System Tests (After JNA Integration)
**Status:** Planned
**Focus:** Full OSPREY integration

**Tests to Implement:**
1. Direct replacement tests
2. OSPREY integration tests
3. Ground truth tests (TestFindGMEC)

### Phase 4: Performance Tests (Ongoing)
**Status:** Planned
**Focus:** Performance validation

**Tests to Implement:**
1. Performance benchmarks
2. Stress tests
3. Correctness under load

## Test Data Sources

### Ground Truth: Expert-Curated, Time-Tested Results

**Philosophy:** When porting or modernizing code, ground truth tests from existing, curated, expert-generated, time-tested results provide the highest value for correctness validation. These tests serve as authoritative references that have been validated through years of use in production.

**Examples:**
- `TestFindGMEC.test1CC8()` - Proven GMEC search with known energy values (-70.617 kcal/mol)
- `TestFindGMEC.testDEEPer()` - Validated DEEPer algorithm results
- Production OSPREY examples (1CC8, 1CC8.deeper, etc.) - Expert-configured test cases

**Value:**
- **Correctness Validation:** Known-good results provide definitive correctness checks
- **Regression Prevention:** Changes that break ground truth tests indicate serious regressions
- **Confidence:** Passing ground truth tests demonstrates the new implementation matches proven behavior
- **Documentation:** Ground truth tests document expected behavior and edge cases

**Implementation:**
- Capture objects from ground truth tests using `tools/capture-test-data.sh osprey`
- Store captured serialized objects in `test-data/serialized/`
- Use captured data in integration tests (`test_captured_data.cpp`)
- Compare C++ deserialization results against known-good Java results

### From OSPREY
- `BenchmarkDeepCopy.java` test classes (SimpleObject, NestedObject, CircularNode)
- `TestFindGMEC` test scenarios (1CC8, DEEPer) - **Ground Truth**
- Production objects (MoleculeModifierAndScorer, EPICMatrix, etc.) - **Ground Truth**

### Generated (Synthetic)
- Algorithm-based test cases (edge cases, stress tests)
- Synthetic object graphs (varying depth, size, complexity)
- Fast development/debugging tests (via `GenerateTestData`)

### Test Data Generation Strategy

**Two-Tier Approach:**

1. **Fast Synthetic Tests** (`tools/capture-test-data.sh synthetic`)
   - Uses `GenerateTestData` standalone main class
   - No OSPREY infrastructure required
   - Fast execution (< 1 second)
   - Purpose: Development, debugging, quick validation
   - Objects: SimpleObject, NestedObject, CircularNode, DeepNested

2. **Ground Truth Tests** (`tools/capture-test-data.sh osprey`)
   - Uses full OSPREY test framework (`CaptureTestObjects`)
   - Requires full build and OSPREY infrastructure
   - Slower execution (minutes)
   - Purpose: Correctness validation against proven results
   - Objects: Real OSPREY objects from `TestFindGMEC`, production code, etc.

**Workflow:**
- Use synthetic tests during development for fast iteration
- Use ground truth tests for correctness validation and before commits
- Both test data types stored in `test-data/` and used by integration tests

## Success Criteria

### Unit Tests
- All stream parsing functions work correctly
- Deserialization nodes managed properly
- Object graph construction correct
- BFS traversal algorithm correct

### Integration Tests
- C++ can parse Java-serialized OSPREY objects
- Deserialized objects match original structure
- References resolved correctly

### System Tests
- `TestFindGMEC.test1CC8()` passes with C++ deep copy
- Results identical to Java deep copy
- Performance improvement (2-5x faster)
- No stack overflow (unlimited depth)

## Test Execution

### C++ Unit Tests
```bash
cd osprey-fork/src/main/cc/DeepCopy
./build-and-test.sh
```

### Java Integration Tests
```bash
cd osprey-fork
./gradlew test --tests "edu.duke.cs.osprey.tools.TestDeepCopy*"
```

### System Tests (Ground Truth)
```bash
cd osprey-fork
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC"
```

## Test Maintenance

- Add tests as features are implemented (TDD approach)
- Update tests when OSPREY test cases change
- Maintain test coverage > 80%
- Document test derivation (which OSPREY test → which C++ test)
- **Prioritize ground truth tests** - When in doubt, use proven OSPREY test cases as the source of truth

## Ground Truth Testing Philosophy

### Why Ground Truth Tests Matter

When modernizing or porting code, **ground truth tests from existing, curated, expert-generated, time-tested results provide the highest value for correctness validation**. This is especially critical when:

1. **Porting implementations** (Java → C++)
2. **Modernizing code** (recursive → iterative algorithms)
3. **Optimizing performance** (ensuring correctness is maintained)
4. **Refactoring** (validating behavior preservation)

### Ground Truth Test Sources

**OSPREY Examples:**
- `TestFindGMEC.test1CC8()` - 7-residue GMEC search with known energy (-70.617 kcal/mol)
- `TestFindGMEC.testDEEPer()` - 4-residue DEEPer search with validated results
- Production examples in `examples/1CC8/`, `examples/1CC8.deeper/`, etc.

**Production Code:**
- Objects serialized during actual OSPREY computations
- `MoleculeModifierAndScorer` from SAPE.java
- `EPICMatrix` from VoxelGCalculator
- Other production objects with proven correctness

### Test Data Capture Infrastructure

The project provides two modes for test data generation:

1. **Synthetic Mode** (fast, for development):
   ```bash
   ./tools/capture-test-data.sh synthetic
   ```
   - Generates simple test objects (SimpleObject, NestedObject, etc.)
   - No OSPREY infrastructure required
   - Fast execution for rapid iteration

2. **OSPREY Mode** (ground truth, for validation):
   ```bash
   ./tools/capture-test-data.sh osprey
   ```
   - Captures objects from real OSPREY tests
   - Requires full OSPREY build and test framework
   - Slower but provides authoritative test data

### Integration with Testing Strategy

Ground truth tests are integrated at multiple levels:

- **Level 2 (Integration Tests):** Use captured serialized objects to validate C++ deserialization
- **Level 3 (System Tests):** Compare C++ deep copy results against known-good Java results
- **Level 4 (Performance Tests):** Ensure performance improvements don't break correctness

This ensures that the C++ implementation maintains correctness while achieving performance goals.

