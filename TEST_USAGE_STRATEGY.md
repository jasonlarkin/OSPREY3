# Test Usage Strategy

## Available Test Outputs

### Test Results Location
- `test-results-20251206-221708/` - Test execution logs from Dec 6, 2025
- `examples/*/` - Example output files (`.outputconfs.txt`, `.results.txt`)
- `trace_output.log` - Trace execution output

### Test Execution Status

**TestFindGMEC (Dec 6, 2025):**
- `test1CC8()` - PASSED
- `testDEEPer()` - PASSED
- Duration: 12m 20s total

**Example Outputs Found:**
- `examples/1CC8/1CC8.outputconfs.txt` - GMEC result: Energy -70.617, Assignments [5, 7, 12, 5, 0, 7, 4]
- `examples/python.EWAKStar/ewakStar.results.txt` - KStar results with sequences and scores
- `examples/2RL0.kstar/*.txt` - KStar output files

## What We Can Do With These Tests

### 1. Baseline Validation

**Purpose:** Establish ground truth for C++ deep copy correctness

**Process:**
1. Run tests on base branch to capture:
   - Serialized objects (already done: `test-data/serialized/1cc8-deeper-molecule-modifier-scorer.bin`)
   - Expected GMEC results (energy, assignments)
   - Expected intermediate states

2. Compare C++ deserializer results against Java baseline:
   ```bash
   # Run validation test
   ./gradlew test --tests "edu.duke.cs.osprey.tools.TestDeepCopyValidation"
   ```

3. Validate against captured outputs:
   - Energy values match within tolerance
   - Assignments match exactly
   - Object graph structure preserved

### 2. Performance Benchmarking

**Purpose:** Measure C++ deep copy performance vs Java

**Metrics to Capture:**
- Deserialization time (Java vs C++)
- Memory usage
- Throughput (objects/second)

**Process:**
```bash
# Run benchmark
./gradlew test --tests "edu.duke.cs.osprey.tools.BenchmarkDeepCopy"

# Compare with Java baseline
java -cp build/classes/java/main edu.duke.cs.osprey.tools.BenchmarkDeepCopy
```

### 3. Test Coverage Analysis

**Purpose:** Identify which code paths are exercised by tests

**Tools:**
- Use `CODE_ANALYSIS_TOOLS.md` for coverage analysis
- Run with coverage flags:
  ```bash
  ./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC" \
    -Dorg.gradle.jvmargs="-Xmx8g" \
    -Dtest.coverage=true
  ```

**Analysis:**
- Which algorithms are tested (A*, DEE, EPIC, LUTE)
- Which energy calculation paths
- Which serialization/deserialization paths
- Edge cases covered

### 4. Step-by-Step Execution Analysis

**Purpose:** Understand where execution stops (13/17 steps)

**Investigation:**
1. Check SOFEA step execution (pass1step, pass2step)
2. Check Coffee step execution
3. Identify termination conditions

**Tools:**
```bash
# Run with verbose logging
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8" --info

# Check for step logging
grep -i "step" test-results-*/edu_duke_cs_osprey_gmec_TestFindGMEC.log
```

**Possible Causes for 13/17 Steps:**
- Termination criterion met early
- Time limit reached
- Memory limit reached
- Convergence threshold met
- Error/exception during execution

### 5. Data Capture for C++ Testing

**Purpose:** Generate test data for C++ deserializer validation

**Already Captured:**
- `test-data/serialized/1cc8-deeper-molecule-modifier-scorer.bin` (219KB)
- Object: `MoleculeModifierAndScorer`
- Context: `SAPE.java:93` → `ObjectIO.deepCopy()`

**Additional Captures Needed:**
1. Run full test suite and capture all deep copy calls:
   ```bash
   # Instrument ObjectIO.deepCopy() to capture all calls
   ./gradlew test --tests "edu.duke.cs.osprey.gmec.*"
   ```

2. Capture expected results:
   - GMEC energies
   - Assignments
   - Intermediate states

3. Store in `test-data/`:
   - `serialized/` - Binary objects
   - `expected/` - Expected outputs (JSON/text)
   - `metadata/` - Object metadata

### 6. Regression Testing

**Purpose:** Ensure C++ changes don't break existing functionality

**Process:**
1. Run full test suite before changes
2. Capture baseline results
3. Make C++ changes
4. Re-run tests and compare

**Test Suites:**
```bash
# All GMEC tests
./gradlew test --tests "edu.duke.cs.osprey.gmec.*"

# All energy tests
./gradlew test --tests "edu.duke.cs.osprey.energy.*"

# All KStar tests
./gradlew test --tests "edu.duke.cs.osprey.kstar.*"

# Full test suite
./gradlew test
```

### 7. Integration Testing

**Purpose:** Validate C++ deep copy works in full OSPREY workflow

**Test Flow:**
1. Run `TestFindGMEC.test1CC8()` with C++ deep copy enabled
2. Verify:
   - GMEC energy matches baseline (-70.617 kcal/mol)
   - Assignments match ([5, 7, 12, 5, 0, 7, 4])
   - No stack overflow errors
   - Performance acceptable

**Enable C++ Deep Copy:**
- Modify `ObjectIO.deepCopy()` to use C++ implementation
- Or create `ObjectIO.deepCopyNative()` and switch calls

### 8. Edge Case Testing

**Purpose:** Test C++ deserializer with various object types

**Test Cases:**
- Simple objects (primitives, strings)
- Nested objects
- Circular references
- Large objects (like MoleculeModifierAndScorer)
- Arrays (primitive, object)
- Collections (ArrayList, HashMap, etc.)

**Process:**
```bash
# Run capture test to generate test cases
./gradlew test --tests "edu.duke.cs.osprey.tools.CaptureTestObjects"

# Validate each captured object
./gradlew test --tests "edu.duke.cs.osprey.tools.TestDeepCopyValidation"
```

### 9. Comparison with Original Java

**Purpose:** Validate C++ produces identical results

**Comparison Points:**
- Object graph structure
- Field values
- Reference relationships
- Memory layout (where applicable)

**Tools:**
- Java reflection to inspect deserialized objects
- C++ object graph inspection
- Serialization format comparison

### 10. Documentation and Examples

**Purpose:** Use test outputs as documentation

**Create:**
- Example outputs showing expected results
- Test data descriptions
- Performance benchmarks
- Usage examples

## Next Steps

1. **Investigate 13/17 Steps:**
   - Check test logs for termination reason
   - Identify which step failed/stopped
   - Determine if this is expected or a bug

2. **Expand Test Data Capture:**
   - Run full test suite with instrumentation
   - Capture all deep copy calls
   - Store expected results

3. **Set Up Continuous Validation:**
   - Create test script that runs baseline tests
   - Compares C++ results with Java
   - Reports discrepancies

4. **Performance Analysis:**
   - Benchmark Java deep copy
   - Benchmark C++ deep copy
   - Compare and document improvements

5. **Coverage Analysis:**
   - Run tests with coverage
   - Identify untested code paths
   - Add tests for critical paths

## Test Execution Commands

```bash
# Run specific test
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8"

# Run with verbose output
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC" --info

# Run with coverage
./gradlew test --tests "edu.duke.cs.osprey.gmec.*" \
  -Dorg.gradle.jvmargs="-Xmx8g"

# Run all tests
./gradlew test

# Run and capture output
./gradlew test --tests "edu.duke.cs.osprey.gmec.*" > test-output.log 2>&1
```

