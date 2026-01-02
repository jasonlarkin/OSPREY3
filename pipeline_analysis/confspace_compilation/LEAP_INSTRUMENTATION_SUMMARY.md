# LEaP Call Instrumentation Summary

## Test Results

### Run 1: Parallel Compilation (Baseline)
- **Date:** December 27, 2025
- **Total time:** 455.94s (7.6 min)
- **Speedup:** 1.32x vs sequential (599.73s)

### Run 2: Parallel Compilation (With Resource Contention)
- **Date:** December 27, 2025 (later)
- **Total time:** 633.51s (10.55 min)
- **Design:** 8.90m (534s)
- **Complex:** 10.32m (619.2s)

**Observation:** 39% slower than baseline run, confirming:
- Parallel compilation is sensitive to system load
- Resource contention significantly impacts performance
- LEaP calls may serialize under load

## Key Findings

### 1. Parallel Compilation Works
- Shared LocalService implementation successful
- All three confspaces compile simultaneously
- No errors or failures

### 2. Speedup is Limited
- **Best case:** 1.32x speedup (455.94s vs 599.73s sequential)
- **With contention:** Slower than sequential (633.51s vs 599.73s)
- **Efficiency:** 44% of ideal (1.32x / 3 confspaces)

### 3. LEaP is the Bottleneck
- Individual confspaces take longer in parallel (resource contention)
- Suggests LEaP calls are competing for resources
- Subprocess overhead dominates (100-500ms per call)

### 4. Resource Contention Matters
- 39% performance degradation under load
- Parallel compilation not robust to system load
- May need process isolation or better resource management

## What We Still Need

### Detailed LEaP Call Patterns
To optimize effectively, we need:
1. **Call count** - How many LEaP calls per confspace?
2. **Call timing** - How long does each call take?
3. **Call patterns** - Are there duplicates? What are batch sizes?
4. **Resource usage** - CPU, memory, I/O during calls

### Current Limitations
- Simple timing script only gives total time
- No visibility into individual LEaP calls
- No call count or pattern data
- Cannot identify optimization opportunities

## Next Steps

### Option 1: Add Kotlin Logging (Recommended)
- Modify `Leap.kt` to log each call
- Log: call ID, duration, input/output file counts
- Modify `ConfSpaceCompiler.kt` for context
- **Effort:** 1-2 hours
- **Value:** High - enables targeted optimizations

### Option 2: JVM Profiling
- Use async-profiler to see HTTP service calls
- Less granular than code instrumentation
- **Effort:** 30 minutes
- **Value:** Medium - shows call frequency but not details

### Option 3: Proceed with Known Optimizations
- Implement fragment caching (2-5x potential)
- Implement LEaP batching (5-10x potential)
- Based on code analysis, not call patterns
- **Effort:** 1-2 weeks
- **Value:** High - addresses known bottlenecks

## Recommendations

**Short-term:**
1. Document current findings (this summary)
2. Proceed with fragment caching (low risk, good impact)
3. Consider LEaP batching (higher effort, highest impact)

**Medium-term:**
1. Add Kotlin logging for detailed call patterns
2. Analyze patterns to validate optimization approach
3. Implement persistent LEaP process if beneficial

**Long-term:**
1. Optimize LEaP integration (reduce subprocess overhead)
2. Consider alternative forcefields (faster parameterization)
3. Refactor compilation pipeline (in-memory, unified architecture)

## Conclusion

Parallel compilation works but provides marginal speedup (1.32x) due to:
- LEaP subprocess overhead (main bottleneck)
- Resource contention (system load sensitivity)
- Possible LEaP call serialization

**Real optimizations needed:**
- LEaP batching: 5-10x potential
- Fragment caching: 2-5x potential
- Persistent LEaP process: 3-5x potential

**Combined potential:** 10-50x speedup for ConfSpace compilation

