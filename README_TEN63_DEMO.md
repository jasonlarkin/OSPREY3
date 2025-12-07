# Ten63 Demo: OSPREY3 Serialization Fixes

## Overview

This repository demonstrates identification and resolution of serialization issues in OSPREY3's official test examples. These fixes address failures that prevented examples from running out-of-the-box.

## Quick Links

- **[Detailed Fix Documentation](SERIALIZATION_FIXES.md)** - Complete technical analysis
- **[GitHub Actions Workflow](https://github.com/jasonlarkin/OSPREY3/actions/workflows/test.yml)** - Automated test runs
- **[Latest Workflow Runs](https://github.com/jasonlarkin/OSPREY3/actions)** - View all test executions
- **[Branch: ten63-demo/serialization-fixes](https://github.com/jasonlarkin/OSPREY3/tree/ten63-demo/serialization-fixes)** - Demo branch with fixes

## Problem Summary

Official OSPREY3 test examples (`TestFindGMEC`) failed with:
1. `StackOverflowError` - Java serialization recursion depth exceeded stack limits
2. `NotSerializableException` - Missing `Serializable` interface on 11+ Amber forcefield classes

## Solution Summary

**Fix 1:** Increased JVM stack size to 8MB in `build.gradle.kts`
- Resolves recursive serialization stack overflow

**Fix 2:** Implemented `Serializable` on all Amber forcefield parameter classes
- 12 files modified (11 parameter classes + `ForcefieldFileParser`)
- Added `transient` keyword to non-serializable fields

## Test Results

| Test | Before | After |
|------|--------|-------|
| `test1CC8()` | FAILED | PASSED (8m 43s) |
| `testDEEPer()` | FAILED | PASSED (3m 40s) |

## Running Tests

Verify fixes locally:

```bash
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC"
```

### Automated Testing (GitHub Actions)

Tests are automatically executed via GitHub Actions on every push:

- **Workflow**: [Test OSPREY3 Serialization Fixes](https://github.com/jasonlarkin/OSPREY3/actions/workflows/test.yml)
- **Latest Runs**: [All Workflow Runs](https://github.com/jasonlarkin/OSPREY3/actions)
- **Configuration**: [`.github/workflows/test.yml`](.github/workflows/test.yml)

The workflow demonstrates:
- Initial test failures (before fixes)
- Progressive improvement (after Fix #1)
- Final success (after Fix #2)

Each run includes:
- Test execution summary
- Detailed logs as artifacts
- HTML test reports

## Key Findings

### Current Java Implementation Issues

1. **Runtime Discovery** - Serialization errors only appear at runtime, not compile-time
2. **Maintenance Burden** - Every class must explicitly implement `Serializable`
3. **Stack Limitations** - Recursive serialization causes stack overflow for deep object graphs
4. **Brittleness** - One missing class breaks entire serialization chain

### Modern C++ Advantages

1. **Compile-time Safety** - Type system catches errors during compilation
2. **Explicit Control** - Custom serialization formats, no automatic graph traversal
3. **No Stack Limits** - Iterative algorithms, explicit stack management
4. **Move Semantics** - Efficient copying without serialization overhead
5. **Performance** - Zero-cost abstractions, better memory management

## About AMBER Force Fields

The affected classes implement AMBER (Assisted Model Building with Energy Refinement) force fields:
- Widely used molecular dynamics software package (originally UCSF)
- Family of force fields for biomolecular simulation
- Used across many computational platforms

OSPREY integrates Amber parameters for protein energy calculations. The implementation includes code copyright 2001-2018 by the Bruce Donald Lab at Duke University. The Amber parameter classes were added to OSPREY3 in 2021 (commit [adb27be](https://github.com/donaldlab/OSPREY3/commit/adb27be298bd57b8d159a41b90884bea9803e3bc)).

References:
- [AMBER Force Fields](https://ambermd.org/AmberModels.php)
- [AMBER (Wikipedia)](https://en.wikipedia.org/wiki/AMBER)

## Branch Strategy

- `main` - Original OSPREY3 code (fails)
- `ten63-demo/serialization-fixes` - Fixes applied incrementally with commits

Each commit demonstrates:
1. Initial failure (GitHub Actions shows errors) - [View example run](https://github.com/jasonlarkin/OSPREY3/actions)
2. Fix #1 (stack size) - partial resolution (StackOverflowError fixed)
3. Fix #2 (Serializable) - full resolution (all tests passing)

**GitHub Actions Status:**
- Workflow triggers on push to `ten63-demo/**` branches
- Runs `TestFindGMEC` test suite on JDK 17
- Provides detailed test summaries and artifact downloads

## Files Modified

- `build.gradle.kts` - Stack size configuration
- `src/main/java/edu/duke/cs/osprey/energy/forcefield/amber/*.java` - 12 files with Serializable implementations

View changes:
```bash
git diff main..ten63-demo/serialization-fixes --stat
```

## Documentation

- **Technical Details:** See [SERIALIZATION_FIXES.md](SERIALIZATION_FIXES.md)
- **OSPREY Docs:** See [doc/content/contributing/fixes/serialization-fixes.md](doc/content/contributing/fixes/serialization-fixes.md)

## Contact

For questions about this demo, see the [detailed documentation](SERIALIZATION_FIXES.md) or review the commit history on the demo branch.

