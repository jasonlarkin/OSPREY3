# Performance Benchmarking Note

## Timing Code Status

The performance benchmarking relies on timing code in `TestNativeConfEnergyCalculator.java` that logs execution times:

```java
log("assign: %d confs in %s (%.2f confs/s)", confs.length, stopwatch.getTime(2), confs.length/stopwatch.getTimeS());
log("calcEnergy_all: %d confs in %s (%.2f confs/s)", ...);
log("minimizeEnergy_all: %d confs in %s (%.2f confs/s)", ...);
```

**Current Status:**
- **develop branch**: Timing code exists (added in C++20 modernization work)
- **main branch**: May not have timing code (needs verification)

## Solutions

### Option 1: Use Total Test Execution Time (Fallback)

The benchmarking script now falls back to using total Gradle test execution time if detailed timing logs are not found. This provides a basic comparison but is less precise.

### Option 2: Add Timing Code to Main Branch

If main branch doesn't have timing code, you can:

1. **Temporarily add it** for benchmarking:
   ```bash
   # In main branch
   # Add Stopwatch timing to TestNativeConfEnergyCalculator methods
   # (Copy from develop branch)
   ```

2. **Or use a patch file**:
   ```bash
   # Create patch from develop
   git diff main develop -- src/test/java/edu/duke/cs/osprey/energy/compiled/TestNativeConfEnergyCalculator.java > timing.patch
   
   # Apply to main temporarily
   git apply timing.patch
   ```

### Option 3: Use Different Benchmark Approach

Instead of relying on test timing, use:
- Dedicated benchmark programs
- Profiling tools (perf, valgrind)
- External timing wrappers

## Current Implementation

The `extract_performance_metrics.py` script:
1. First tries to extract detailed timing from log output
2. Falls back to total test execution time if detailed timing not found
3. Reports which method was used

## Recommendation

For accurate performance comparison:
1. Verify if main branch has timing code
2. If not, temporarily add it (or use the fallback)
3. Ensure `-PshowTestOutput=true` is set in both branches (or configure `showStandardStreams` in build.gradle.kts)

## Quick Check

```bash
# Check if main branch has timing code
grep -q "log(\"assign:.*confs in" ../osprey-fork-main/src/test/java/edu/duke/cs/osprey/energy/compiled/TestNativeConfEnergyCalculator.java && echo "Has timing" || echo "Missing timing"
```

If missing, the script will use total test execution time as fallback.

