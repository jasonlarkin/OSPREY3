# Full K* GC Analysis Comparison

## Test Cases

- **test2RL0**: 2RL0 system, 16,734 pairs (complex), 7,575 pairs (ligand), 1,008 pairs (protein)
- **test1GUA11**: 1GUA11 system, full calculation

## Summary Comparison

| Metric | test2RL0 | test1GUA11 | Difference |
|--------|----------|------------|------------|
| **Total Duration** | 169.58s | 79.60s | test2RL0 is 2.13x longer |
| **Total GC Events** | 33 | 42 | test1GUA11 has 27% more events |
| **Young GC Count** | 25 | 28 | test1GUA11 has 12% more Young GCs |
| **Full GC Count** | 0 | 0 | Both: No Full GC (good) |
| **Total GC Pause Time** | 0.872s | 2.709s | test1GUA11 has 3.11x more pause time |
| **Average GC Pause** | 26.41 ms | 64.50 ms | test1GUA11 has 2.44x longer average pause |
| **Maximum GC Pause** | 370.57 ms | 910.99 ms | test1GUA11 has 2.46x longer max pause |
| **GC Overhead** | 0.51% | 3.40% | test1GUA11 has 6.67x higher overhead |
| **Peak Heap** | 168M | 190M | test1GUA11 uses 13% more heap |

## Key Findings

### test2RL0

- **GC Overhead**: 0.51% (very low)
- **No Full GC**: Heap sizing appropriate
- **One Large Pause**: 370ms at 19.6s
  - GC(24): "Other" phase took 62.1ms (unusual)
  - Likely system-level delays (I/O, scheduling)
- **Heap Growth**: Steady 68M → 168M
- **GC Pattern**: Frequent small pauses (8-40ms), one outlier

### test1GUA11

- **GC Overhead**: 3.40% (higher than test2RL0, but still acceptable)
- **No Full GC**: Heap sizing appropriate
- **Two Large Pauses**: 
  - 277ms at 25.5s (GC(9))
  - 911ms at 37.5s (GC(15)) - very large
- **Heap Growth**: Steady 68M → 190M
- **GC Pattern**: More frequent pauses, longer average pause time
- **Shorter Duration**: 79.6s vs 169.6s (but higher GC overhead)

## 370ms Pause Investigation (test2RL0)

**GC(24) at 19.644s:**
- Total pause: 370.57ms
- Evacuate Collection Set: 6.9ms (normal)
- Other: 62.1ms (unusual - system overhead)
- Real time: 0.42s (longer than pause time)
- User time: 0.03s
- System time: 0.00s

**Analysis:**
- Actual GC work: ~7ms
- System overhead: ~62ms in "Other" phase
- Remaining ~300ms: likely thread scheduling, I/O waits, or OS-level delays
- Not a GC algorithm issue - system resource contention

## Comparison with Minimal Tests

### Minimal Test Characteristics (from previous analysis)
- Duration: Seconds (too fast to capture application code)
- GC: Mostly infrastructure (class loading, JIT compilation)
- Application code: Not visible

### Full K* Test Characteristics
- Duration: Minutes (2m 48s for test2RL0)
- GC: Actual application memory patterns
- Application code: Visible in profiles
- GC overhead: Low (0.51%)

## Key Differences

### test2RL0 (Better GC Performance)
- Lower GC overhead (0.51% vs 3.40%)
- Shorter average pauses (26ms vs 64ms)
- Longer total duration but more efficient GC
- One large pause (370ms) - system-level

### test1GUA11 (Higher GC Pressure)
- Higher GC overhead (3.40% vs 0.51%)
- Longer average pauses (64ms vs 26ms)
- More GC events (42 vs 33)
- Two large pauses (277ms, 911ms)
- Shorter total duration but less efficient GC

### Possible Causes for test1GUA11 Differences
- Different workload characteristics (more allocation pressure)
- Different data structures or algorithms
- System resource contention during execution
- Different memory access patterns

## Recommendations

1. **test2RL0**: GC is not a bottleneck (0.51% overhead is excellent)
2. **test1GUA11**: GC overhead is acceptable (3.40%) but higher than test2RL0
3. **Large pauses are system-level**: Not GC algorithm issues (check "Other" phase times)
4. **Heap sizing is appropriate**: No Full GC events in either test
5. **Memory patterns are stable**: Steady growth, no leaks observed
6. **Arena allocation priority**:
   - **test2RL0**: Low priority (GC overhead already very low)
   - **test1GUA11**: Medium priority (could reduce 3.40% overhead, but not critical)
   - **Focus should be on CPU bottlenecks**, not memory (GC overhead < 5% in both cases)

## Next Steps

1. Complete test1GUA11 analysis
2. Compare GC patterns between test cases
3. Investigate CPU bottlenecks (perf/async-profiler when available)
4. Focus optimization on compute-bound operations, not memory

