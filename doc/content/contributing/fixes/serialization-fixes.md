+++
menuTitle = "Serialization Fixes"
title = "Serialization Fixes for Official Examples"
weight = 99
+++

# Serialization Fixes for OSPREY3 Official Examples

## Overview

This document describes fixes applied to resolve serialization issues in OSPREY3's official test examples (`TestFindGMEC`). These fixes address failures that prevented the examples from running out-of-the-box.

## Problem Discovery

Official OSPREY3 examples failed to execute:
- `TestFindGMEC.test1CC8()` - FAILED
- `TestFindGMEC.testDEEPer()` - FAILED

Two distinct errors were encountered:

1. **StackOverflowError** - During deep object copying (EPIC matrix initialization)
2. **NotSerializableException** - When caching energy matrices to disk

## Root Cause Analysis

### Issue 1: Stack Overflow

Java's default serialization uses recursion to traverse object graphs. For complex molecular structures with deep interlinked references, recursion depth exceeds the default JVM stack size (typically 1MB).

**Location:** `ObjectIO.deepCopy()` → `ematrix.epic.SAPE` initialization

### Issue 2: Missing Serializable Implementation

Java serialization requires every class in an object graph to implement the `Serializable` interface. Multiple classes in the Amber forcefield parameter system were missing this implementation.

**Classes affected:**
- `AtomSymbolAndMass` (core data type)
- 10 parameter record classes
- `ForcefieldFileParser` (also contained non-serializable fields)

## About AMBER Force Fields

The affected classes are part of OSPREY's Amber force field implementation. [AMBER (Assisted Model Building with Energy Refinement)](https://en.wikipedia.org/wiki/AMBER) is:

- A widely used molecular dynamics software package originally developed by Peter Kollman's group at UCSF
- A family of force fields for molecular dynamics of biomolecules
- Used across many computational platforms

OSPREY integrates Amber force field parameters for protein energy calculations. These parameter classes store:
- Atom types and masses
- Bond length and angle parameters
- Dihedral parameters
- Van der Waals radii
- Other molecular force field data

These files were added to OSPREY3 in a large commit ([adb27be](https://github.com/donaldlab/OSPREY3/commit/adb27be298bd57b8d159a41b90884bea9803e3bc)) in 2021, implementing the Amber force field system.

Reference: [AMBER Force Fields](https://ambermd.org/AmberModels.php)

## Fixes Applied

### Fix 1: Increase JVM Stack Size

**File:** `build.gradle.kts`

**Change:** Added `jvmArgs("-Xss8m")` to test task configuration

```kotlin
tasks.withType<Test> {
    // ...
    jvmArgs(Jvm.moduleArgs)
    jvmArgs("-Xss8m")  // Increase stack size to 8MB
}
```

**Impact:** Resolves StackOverflowError for deep object copying

### Fix 2: Implement Serializable on Parameter Classes

**Files:** 12 files in `src/main/java/edu/duke/cs/osprey/energy/forcefield/amber/`

**Changes:**

1. **AtomSymbolAndMass.java**
   - Added `import java.io.Serializable;`
   - Added `implements Serializable` to record

2. **Parameter Record Classes (10 files):**
   - `BondLengthParameter.java`
   - `BondAngleParameter.java`
   - `DihederalParameter.java`
   - `ImproperDihederalParameter.java`
   - `HBond10_12PotentialParameter.java`
   - `SlaterKirkwoodParameter.java`
   - `Six12PotentialParameter.java`
   - `EquivalencingAtom.java`
   - `VanDerWaalsRadius.java`
   - `Six12PotentialCoefficient.java`
   
   For each: Added `import java.io.Serializable;` and `implements Serializable`

3. **ForcefieldFileParser.java**
   - Added `import java.io.Serializable;`
   - Added `implements Serializable` to class
   - Added `serialVersionUID = 1L`
   - Made `InputStream parmFile` and `Path frcmod` fields `transient`

**Change Summary:**
- 12 files modified
- 30 lines added, 14 lines removed
- All changes: Adding `Serializable` interface and `transient` keyword

## Test Results

### Automated Testing via GitHub Actions

All fixes are verified through automated testing using GitHub Actions. The workflow (`.github/workflows/test.yml`) runs on every push to `ten63-demo/**` branches and `main`.

**GitHub Actions:**
- Workflow: [Test OSPREY3 Serialization Fixes](https://github.com/jasonlarkin/OSPREY3/actions/workflows/test.yml)
- Runs: [All workflow runs](https://github.com/jasonlarkin/OSPREY3/actions)
- Branch: `ten63-demo/serialization-fixes`

The workflow:
1. Runs `TestFindGMEC` test suite on JDK 17
2. Parses and summarizes test results in workflow summary
3. Uploads test reports and logs as artifacts
4. Captures both failures and successes for demonstration

### Before Fixes
- `test1CC8()`: FAILED (StackOverflowError, NotSerializableException)
- `testDEEPer()`: FAILED (NotSerializableException)
- **GitHub Actions**: Shows failure status with error logs

### After Fix 1 (Stack Size Only)
- `test1CC8()`: FAILED (NotSerializableException only)
- StackOverflowError: RESOLVED
- **GitHub Actions**: Shows partial fix - StackOverflowError resolved, but NotSerializableException persists

### After Fix 2 (Serializable Implementation)
- `test1CC8()`: PASSED (8m 43s)
- `testDEEPer()`: PASSED (3m 40s)
- **GitHub Actions**: Shows all tests passing with green checkmark

## Why This Demonstrates Need for Modern C++

### Current Issues (Java Serialization)

1. **Maintenance Burden**
   - Every class must implement Serializable
   - 11+ classes were missing this requirement
   - Easy to forget when adding new classes

2. **Runtime Discovery**
   - No compile-time checking
   - Issues only appear when actually serializing
   - Failures occur deep in call stack

3. **Brittleness**
   - One missing class breaks entire serialization
   - Must mark non-serializable fields as `transient`
   - Discovered only at runtime

4. **Stack Limitations**
   - Recursive serialization causes stack overflow
   - Requires workaround (larger stack size)
   - Default settings insufficient for complex structures

### Modern C++ Advantages

1. **Explicit Control**
   - Choose what/how to serialize
   - Custom serialization formats (JSON, binary, etc.)
   - No automatic traversal of object graph

2. **Compile-Time Safety**
   - Template-based checks catch errors early
   - Type system prevents mistakes
   - Errors discovered at compile time

3. **No Stack Limits**
   - Iterative algorithms avoid recursion
   - Explicit stack/queue management
   - No recursion depth limitations

4. **Move Semantics**
   - Efficient copying without serialization
   - Zero-cost abstractions
   - Better performance

5. **Type Safety**
   - Strong typing prevents serialization errors
   - Explicit control over data structures

## Verification

### Local Testing

Run tests to verify fixes:

```bash
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC"
```

Expected: Both tests pass.

### Automated Testing

Tests are automatically run via GitHub Actions on every push. View results:
- **Latest run**: [GitHub Actions - Latest Run](https://github.com/jasonlarkin/OSPREY3/actions)
- **Workflow details**: [`.github/workflows/test.yml`](.github/workflows/test.yml)

Each workflow run provides:
- Test execution summary in workflow summary
- Detailed test logs as downloadable artifacts
- Exit codes for CI/CD integration
- Test reports in HTML format

## Files Modified

- `build.gradle.kts` (1 file, stack size configuration)
- `src/main/java/edu/duke/cs/osprey/energy/forcefield/amber/*.java` (12 files, Serializable implementations)

## Related GitHub Issues

A search of the [donaldlab/OSPREY3 issues](https://github.com/donaldlab/OSPREY3/issues) repository was performed to identify if these problems were previously reported:

### Search Results

**StackOverflowError:**
- No open or closed issues found for "StackOverflowError", "stack overflow", or "stackoverflow"

**Serialization Issues:**
- **#199** (Closed): "hashcode error on writing the 7th ensemble structure file" - Different issue related to `hashCode()` changing after serialization in MapDB with Sequence objects. Root cause was incorrect YAML configuration marking wildtype as mutable.
- **#115** (Closed): "Error in OSPREY3/examples/python.GMEC/findGMEC.cats.py" - Related to serialization but different error context

**Amber Forcefield Files:**
- 14 issues found mentioning "ForcefieldFileParser", "Amber forcefield", or related terms
- None directly address missing `Serializable` implementations
- Most relate to forcefield usage, configuration, or different error types

**Conclusion:** The specific issues addressed here (missing `Serializable` implementations in Amber parameter classes and stack overflow during deep copying) do not appear to have been reported as separate issues in the OSPREY3 repository.

## References

- [AMBER Force Fields](https://ambermd.org/AmberModels.php)
- [AMBER (Wikipedia)](https://en.wikipedia.org/wiki/AMBER)
- [OSPREY3 Commit: Amber Force Field Implementation](https://github.com/donaldlab/OSPREY3/commit/adb27be298bd57b8d159a41b90884bea9803e3bc)
- [OSPREY3 Issues: Serialization Related](https://github.com/donaldlab/OSPREY3/issues/199)
- [GitHub Actions Workflow](https://github.com/jasonlarkin/OSPREY3/actions/workflows/test.yml)

## Git History

View changes:
```bash
git diff main..ten63-demo/serialization-fixes --stat
git diff main..ten63-demo/serialization-fixes
```

