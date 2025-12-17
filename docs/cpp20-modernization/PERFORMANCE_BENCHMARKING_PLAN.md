# Performance Benchmarking Plan

## Current State

The `TestNativeConfEnergyCalculator` test suite does **not** currently report performance metrics. It only:
- Validates energy calculation correctness
- Logs energy values for debugging
- Asserts energy values match expected results

## Available Tools

The codebase has timing utilities:
- `edu.duke.cs.osprey.tools.Stopwatch` - Simple timing utility
- `edu.duke.cs.osprey.Benchmark` - More sophisticated benchmarking with warmup runs

## Proposed Solution

### Option 1: Add Performance Logging to Existing Tests

Modify `TestNativeConfEnergyCalculator` to:
1. Measure execution time for each test method
2. Log timing information (can be captured from test output)
3. Compare against baseline from main branch

**Pros**: 
- Minimal changes
- Works with existing test infrastructure
- Metrics captured from Gradle test output

**Cons**:
- Test execution time includes JVM startup, test setup, etc.
- Less precise than dedicated benchmarks

### Option 2: Create Dedicated Performance Test

Create a new test class that:
1. Runs multiple iterations of each operation
2. Includes warmup runs
3. Reports average/min/max times
4. Compares against baseline

**Pros**:
- More accurate measurements
- Can control for JVM warmup
- Better for detecting small regressions

**Cons**:
- Requires new test infrastructure
- More complex to maintain

### Option 3: GitHub Actions Performance Comparison

Enhance the GitHub Actions workflow to:
1. Run tests on both main and feature branch
2. Extract timing information from test execution
3. Compare and report differences
4. Fail if performance degrades beyond threshold

**Pros**:
- Automated comparison
- Historical tracking
- Catches regressions in CI

**Cons**:
- CI environment variability
- Requires parsing test output

## Recommended Approach

Combine Option 1 and Option 3:

1. **Add minimal timing to existing tests**:
   - Wrap key operations in `Stopwatch`
   - Log timing to test output (can be captured)
   - Keep it simple - just total time per test method

2. **Enhance GitHub Actions workflow**:
   - Extract timing from test output
   - Compare feature branch vs main branch
   - Report differences in PR comments
   - Set threshold (e.g., fail if >5% slower)

## Implementation Example

### Modified Test Method
```java
@Test 
public void calc_native_all_2RL0_f32() { 
    Stopwatch stopwatch = new Stopwatch().start();
    calc_native_all(confSpace_2RL0, confs_2RL0, calcEnergy_all_2RL0, Structs.Precision.Float32, 1e-5);
    stopwatch.stop();
    log("calc_native_all_2RL0_f32: %s", stopwatch.getTime(2));
}
```

### GitHub Actions Enhancement
```yaml
- name: Extract and compare performance
  run: |
    # Extract timing from test output
    grep "calc_native_all.*:" test-output.log | tee timings.txt
    
    # Compare with main branch baseline
    # Requires baseline storage or fetch from main branch
```

## Baseline Collection

To compare against main branch:
1. Run tests on main branch and capture timings
2. Store baseline in file or artifact
3. Compare feature branch against baseline
4. Report differences

## Next Steps

1. Add timing to a few key test methods
2. Verify timing output is captured in Gradle test logs
3. Enhance GitHub Actions to extract and compare
4. Set up baseline collection mechanism

