# Fast Tests for Development

## Overview

This document categorizes OSPREY tests by execution speed to enable fast development iteration. Tests are grouped into: **Fast (< 5s)**, **Medium (5-30s)**, and **Slow (> 30s)**.

---

## Fast Tests (< 5 seconds)

### Unit Tests - Tools & Utilities

**TestMathTools**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestMathTools.java`
- **Execution Time:** < 1s
- **Tests:** Math utility functions (rounding, precision, etc.)
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestMathTools"`

**TestTimeTools**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestTimeTools.java`
- **Execution Time:** < 1s
- **Tests:** Time formatting and utilities
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestTimeTools"`

**TestPrefixTree**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestPrefixTree.java`
- **Execution Time:** < 1s
- **Tests:** Prefix tree data structure
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestPrefixTree"`

**TestExpFunction**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestExpFunction.java`
- **Execution Time:** < 1s
- **Tests:** Exponential function implementations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestExpFunction"`

**TestJvmMem**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestJvmMem.java`
- **Execution Time:** < 1s
- **Tests:** JVM memory utilities
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestJvmMem"`

**TestBigExp**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestBigExp.java`
- **Execution Time:** < 2s
- **Tests:** BigExp arithmetic operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestBigExp"`

**TestBigDecimalMathThreadSafety**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestBigDecimalMathThreadSafety.java`
- **Execution Time:** < 2s
- **Tests:** Thread safety of BigDecimal operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestBigDecimalMathThreadSafety"`

**TestDihedrals**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/TestDihedrals.java`
- **Execution Time:** < 2s
- **Tests:** Dihedral angle calculations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.TestDihedrals"`

### Unit Tests - Structure

**TestAtom**
- **Location:** `src/test/java/edu/duke/cs/osprey/structure/TestAtom.java`
- **Execution Time:** < 1s
- **Tests:** Atom class functionality
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.structure.TestAtom"`

**TestStrand**
- **Location:** `src/test/java/edu/duke/cs/osprey/structure/TestStrand.java`
- **Execution Time:** < 2s
- **Tests:** Strand structure operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.structure.TestStrand"`

**TestOMOLIO**
- **Location:** `src/test/java/edu/duke/cs/osprey/structure/TestOMOLIO.java`
- **Execution Time:** < 2s
- **Tests:** OMOL file I/O
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.structure.TestOMOLIO"`

### Unit Tests - Tools & Analysis

**RunAnalysis**
- **Location:** `src/test/java/edu/duke/cs/osprey/tools/RunAnalysis.java`
- **Execution Time:** < 1s
- **Tests:** Runs analysis tool on captured data
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.tools.RunAnalysis"`

### Unit Tests - Data Structures

**TestSeqDB**
- **Location:** `src/test/java/edu/duke/cs/osprey/sofea/TestSeqDB.java`
- **Execution Time:** < 3s
- **Tests:** Sequence database operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.sofea.TestSeqDB"`

**TestFringeDB**
- **Location:** `src/test/java/edu/duke/cs/osprey/sofea/TestFringeDB.java`
- **Execution Time:** < 3s
- **Tests:** Fringe database operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.sofea.TestFringeDB"`

**TestBigDecimalIO**
- **Location:** `src/test/java/edu/duke/cs/osprey/sofea/TestBigDecimalIO.java`
- **Execution Time:** < 2s
- **Tests:** BigDecimal serialization
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.sofea.TestBigDecimalIO"`

### Unit Tests - Parallelism

**TestGenerator**
- **Location:** `src/test/java/edu/duke/cs/osprey/parallelism/TestGenerator.java`
- **Execution Time:** < 2s
- **Tests:** Generator pattern implementation
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.parallelism.TestGenerator"`

**TestThreadPoolTaskExecutor**
- **Location:** `src/test/java/edu/duke/cs/osprey/parallelism/TestThreadPoolTaskExecutor.java`
- **Execution Time:** < 3s
- **Tests:** Thread pool task execution
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.parallelism.TestThreadPoolTaskExecutor"`

### Unit Tests - ConfSpace

**TestConfSpace**
- **Location:** `src/test/java/edu/duke/cs/osprey/confspace/compiled/TestConfSpace.java`
- **Execution Time:** < 3s
- **Tests:** Compiled conformation space operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.confspace.compiled.TestConfSpace"`

**TestSequence**
- **Location:** `src/test/java/edu/duke/cs/osprey/confspace/TestSequence.java`
- **Execution Time:** < 2s
- **Tests:** Sequence operations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.confspace.TestSequence"`

### C++ Unit Tests (Very Fast)

**ConfEcalc Tests**
- **Location:** `src/main/cc/ConfEcalc/tests/unit/`
- **Execution Time:** < 1s
- **Tests:** Array operations, basic calculations
- **Run:** `cd src/main/cc/ConfEcalc && ./build-and-test.sh`


---

## Medium Tests (5-30 seconds)

### Integration Tests - Energy

**TestSimpleEnergyCalculator**
- **Location:** `src/test/java/edu/duke/cs/osprey/ematrix/TestSimpleEnergyCalculator.java`
- **Execution Time:** 5-10s
- **Tests:** Simple energy calculations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.ematrix.TestSimpleEnergyCalculator"`

**TestSimpleEnergyMatrixCalculator**
- **Location:** `src/test/java/edu/duke/cs/osprey/ematrix/TestSimpleEnergyMatrixCalculator.java`
- **Execution Time:** 5-10s
- **Tests:** Energy matrix calculations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.ematrix.TestSimpleEnergyMatrixCalculator"`

**TestNativeConfEnergyCalculator**
- **Location:** `src/test/java/edu/duke/cs/osprey/energy/compiled/TestNativeConfEnergyCalculator.java`
- **Execution Time:** 10-20s
- **Tests:** C++ energy calculator (52 tests)
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator"`

**TestEnergyComparison**
- **Location:** `src/test/java/edu/duke/cs/osprey/energy/compiled/TestEnergyComparison.java`
- **Execution Time:** 5-10s
- **Tests:** Java vs C++ energy calculator comparison
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestEnergyComparison"`

### Integration Tests - Minimization

**TestMinimization**
- **Location:** `src/test/java/edu/duke/cs/osprey/minimization/TestMinimization.java`
- **Execution Time:** 10-20s
- **Tests:** Energy minimization algorithms
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.minimization.TestMinimization"`

**TestMinimizationStability**
- **Location:** `src/test/java/edu/duke/cs/osprey/minimization/TestMinimizationStability.java`
- **Execution Time:** 5-10s
- **Tests:** Minimization stability
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.minimization.TestMinimizationStability"`

### Integration Tests - Pruning

**TestSimpleDEE**
- **Location:** `src/test/java/edu/duke/cs/osprey/pruning/TestSimpleDEE.java`
- **Execution Time:** 5-10s
- **Tests:** Dead-end elimination pruning
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.pruning.TestSimpleDEE"`

**TestTransitivePruning**
- **Location:** `src/test/java/edu/duke/cs/osprey/pruning/TestTransitivePruning.java`
- **Execution Time:** 5-10s
- **Tests:** Transitive pruning
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.pruning.TestTransitivePruning"`


---

## Slow Tests (> 30 seconds)

### System Tests - GMEC Finding

**TestFindGMEC.test1CC8()**
- **Location:** `src/test/java/edu/duke/cs/osprey/gmec/TestFindGMEC.java`
- **Execution Time:** 5-10 minutes
- **Tests:** Full GMEC finding with 1CC8 example
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8"`
- **Note:** This is our primary ground truth test, but too slow for iteration

**TestFindGMEC.testDEEPer()**
- **Location:** `src/test/java/edu/duke/cs/osprey/gmec/TestFindGMEC.java`
- **Execution Time:** 5-10 minutes
- **Tests:** GMEC finding with DEEPer
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.testDEEPer"`

### System Tests - KStar

**TestKStar**
- **Location:** `src/test/java/edu/duke/cs/osprey/kstar/TestKStar.java`
- **Execution Time:** 10+ minutes
- **Tests:** KStar calculations
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.kstar.TestKStar"`

**TestBBKStar**
- **Location:** `src/test/java/edu/duke/cs/osprey/kstar/TestBBKStar.java`
- **Execution Time:** 10+ minutes
- **Tests:** Branch-and-bound KStar
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.kstar.TestBBKStar"`

### System Tests - SOFEA

**TestSofea**
- **Location:** `src/test/java/edu/duke/cs/osprey/sofea/TestSofea.java`
- **Execution Time:** 5+ minutes (many tests, some very long)
- **Tests:** SOFEA free energy analysis
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.sofea.TestSofea"`
- **Note:** 13/17 steps likely refers to SOFEA pass1step/pass2step

### System Tests - Python Examples

**TestPythonScripts**
- **Location:** `src/test/java/edu/duke/cs/osprey/python/TestPythonScripts.java`
- **Execution Time:** 30+ minutes (runs all Python examples)
- **Tests:** Python script execution
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.python.TestPythonScripts"`

### System Tests - MARKStar

**TestMARKStar**
- **Location:** `src/test/java/edu/duke/cs/osprey/markstar/TestMARKStar.java`
- **Execution Time:** 10+ minutes
- **Tests:** MARKStar algorithm
- **Run:** `./gradlew test --tests "edu.duke.cs.osprey.markstar.TestMARKStar"`

---

## Recommended Fast Test Suites for Development

### Quick Validation Suite (< 10s total)

```bash
# Run all fast unit tests
./gradlew test --tests "edu.duke.cs.osprey.tools.*" \
  --tests "edu.duke.cs.osprey.structure.TestAtom" \
  --tests "edu.duke.cs.osprey.structure.TestStrand"
```

**Tests Included:**
- TestMathTools
- TestTimeTools
- TestPrefixTree
- TestExpFunction
- TestJvmMem
- TestBigExp
- TestAtom
- TestStrand
- RunAnalysis

### C++ Development Suite (< 5s total)

```bash
# Run C++ unit tests only
cd src/main/cc/ConfEcalc && ./build-and-test.sh
```

**Tests Included:**
- All ConfEcalc unit tests (Array operations, basic calculations)
- Very fast, no Java/Gradle overhead

### Energy Calculator Validation (< 30s total)

```bash
# Run energy calculator tests
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestEnergyComparison" \
  --tests "edu.duke.cs.osprey.ematrix.TestSimpleEnergyCalculator"
```

---

## Development Workflow

### During Active Development (Fast Iteration)

1. **C++ Changes:**
   ```bash
   cd src/main/cc/ConfEcalc && ./build-and-test.sh
   ```
   - Fastest feedback (< 5s)
   - No Java compilation

2. **Java Utility Changes:**
   ```bash
   ./gradlew test --tests "edu.duke.cs.osprey.tools.*"
   ```
   - All tool tests
   - Catches basic regressions

3. **Structure/IO Changes:**
   ```bash
   ./gradlew test --tests "edu.duke.cs.osprey.structure.*"
   ```
   - Structure and I/O tests
   - Fast validation

### Before Committing (Medium Validation)

```bash
# Run medium-speed tests
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.*" \
  --tests "edu.duke.cs.osprey.ematrix.*" \
  --tests "edu.duke.cs.osprey.minimization.TestMinimization"
```

### Full Validation (Before Push)

```bash
# Run full test suite (takes 30+ minutes)
./gradlew test
```

---

## Test Execution Tips

### Use Gradle Daemon
```bash
# First run starts daemon (slower)
./gradlew test --tests "..."

# Subsequent runs reuse daemon (faster)
./gradlew test --tests "..."
```

### Compile Only (Syntax Check)
```bash
# Fast syntax checking
./gradlew compileJava compileTestJava
```

### Run Single Test Method
```bash
# Run just one test method
./gradlew test --tests "edu.duke.cs.osprey.tools.TestDeepCopyValidation.testSimpleObject"
```

### Skip Slow Tests
```bash
# Run all tests except slow ones
./gradlew test --exclude-task "test" \
  --tests "edu.duke.cs.osprey.tools.*" \
  --tests "edu.duke.cs.osprey.ematrix.*"
```

---

## Summary

**Fastest for Development:**
- C++ unit tests (ConfEcalc) (< 5s)
- Tool utility tests (< 1s each)
- Structure tests (< 2s each)

**Good for Validation:**
- Energy calculator tests (10-20s)
- Minimization tests (10-20s)
- Pruning tests (5-10s)

**Use Sparingly:**
- TestFindGMEC (5-10 min)
- TestKStar (10+ min)
- TestPythonScripts (30+ min)

**Recommended Daily Workflow:**
1. C++ unit tests during C++ development
2. Tool/structure tests after Java changes
3. Medium tests before committing
4. Full suite before pushing

