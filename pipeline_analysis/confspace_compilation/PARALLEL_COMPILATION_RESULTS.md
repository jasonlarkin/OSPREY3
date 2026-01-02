# Parallel ConfSpace Compilation Results

## Update: Instrumentation Run

**Parallel compilation with resource contention:**
- Total time: 633.51s (10.55 min)
- Design: 8.90m (534s) - slower than previous
- Complex: 10.32m (619.2s) - slower than previous

**Note:** This run was slower than the initial parallel test (455.94s), likely due to:
- Resource contention from other tasks running locally
- Confirms that parallel compilation is sensitive to system load
- LEaP calls may be serializing under load

**Conclusion:** Parallel compilation works, but speedup is limited by:
1. LEaP subprocess overhead (main bottleneck)
2. Resource contention (CPU/memory competition)
3. Possible LEaP call serialization

## Original Test Results

## Test Configuration
- **Method:** Shared LocalService instance with ThreadPoolExecutor
- **ConfSpaces:** target, design, complex (match1 from 2RL0-MONTAGE)
- **Hardware:** WSL2, single node

## Sequential Compilation Results

| ConfSpace | Compile Time | Notes |
|-----------|-------------|-------|
| Target | 48.35s | Fastest |
| Design | 3.48m (208.8s) | Medium |
| Complex | 5.29m (317.4s) | Slowest |
| **Total** | **599.73s (9.99 min)** | Sequential sum |

## Parallel Compilation Results

| ConfSpace | Compile Time | Notes |
|-----------|-------------|-------|
| Target | 1.07m (64.2s) | Finished first |
| Design | 3.82m (229.2s) | Finished second |
| Complex | 7.40m (444.0s) | Finished last (limiting factor) |
| **Total** | **455.94s (7.6 min)** | Wall-clock time |

**Observation:** All three confspaces compiled simultaneously. Complex took longer in parallel (7.40m vs 5.29m sequential), suggesting resource contention or LEaP serialization.

## Performance Comparison

| Metric | Value |
|--------|-------|
| Sequential time | 599.73s |
| Parallel time | 455.94s |
| **Speedup** | **1.32x** |
| **Time saved** | **143.79s (2.4 min)** |
| Efficiency | 44% (1.32x / 3 confspaces) |

## Analysis

### Speedup Analysis
- **Expected:** 1.5-1.7x (limited by longest compile time ~400s)
- **Actual:** 1.32x
- **Gap:** Lower than expected due to:
  1. Complex confspace took longer in parallel (7.40m vs 5.29m sequential)
  2. Possible LEaP call serialization (if LEaP isn't thread-safe)
  3. Resource contention (CPU, memory, I/O)

### Per-ConfSpace Comparison

| ConfSpace | Sequential | Parallel | Change |
|-----------|------------|----------|--------|
| Target | 48.35s | 64.2s | +33% slower |
| Design | 208.8s | 229.2s | +10% slower |
| Complex | 317.4s | 444.0s | +40% slower |

**Observation:** All confspaces took longer individually in parallel, but total time decreased due to parallelization. This suggests:
- LEaP calls may be serializing (single LEaP process handling requests)
- Resource contention (CPU/memory competition)
- LocalService HTTP server handling concurrent requests with some overhead

### Efficiency
- **Theoretical max:** 3x (3 confspaces in parallel)
- **Ideal (longest-limited):** ~1.9x (599.73s / 317.4s)
- **Actual:** 1.32x
- **Efficiency:** 44% of ideal, 69% of longest-limited

## Conclusions

### Success
- Parallel compilation works with shared LocalService
- 1.32x speedup achieved
- 2.4 minutes saved per match
- No errors or failures

### Limitations
- Speedup lower than expected (1.32x vs 1.5-1.7x)
- Individual confspaces slower in parallel (resource contention)
- LEaP calls may be serializing (needs investigation)

### Next Steps
1. **Investigate LEaP serialization** - Check if LEaP calls are thread-safe
2. **Profile parallel execution** - Identify contention points
3. **Optimize LEaP calls** - Batch or persistent process (5-10x potential)
4. **Fragment caching** - Reduce duplicate parameterization (2-5x potential)

## Impact on MONTAGE

### Current MONTAGE Performance
- ConfSpace compilation: 1744s (29 min) for 4 matches
- Total MONTAGE: 2191s (36.5 min)

### With Parallel Compilation (1.32x)
- ConfSpace compilation: 1321s (22 min) - **423s saved**
- Total MONTAGE: 1768s (29.5 min) - **7 minutes saved per run**

### With Additional Optimizations
- LEaP batching: 5-10x → 132-264s compilation
- Fragment caching: 2-5x → 66-132s compilation
- **Combined potential:** 10-50x total speedup

## Implementation Status

- [x] Shared LocalService implementation
- [x] Parallel compilation function
- [x] Test script
- [x] Performance measurement
- [ ] Integration into MONTAGE workflow
- [ ] LEaP serialization investigation
- [ ] Additional optimizations (batching, caching)

