# Combined Profile Analysis: test1GUA11

Generated: 2025-12-26 06:00:29

## Executive Summary

- **GC Overhead**: 3.40%
- **Total GC Events**: 42
- **Max GC Pause**: 910.99 ms
- **Peak Heap**: 0M

## GC Analysis

| Metric | Value |
|--------|-------|
| Total GC Events | 42 |
| Young GC | 28 |
| Full GC | 0 |
| Total Pause Time | 2.709 s |
| Average Pause | 64.50 ms |
| Maximum Pause | 910.99 ms |
| GC Overhead | 3.40% |
| Peak Heap | 0M |
| Total Duration | 79.60 s |

## Correlations and Insights

- GC overhead is acceptable (< 5%) - memory optimization has low priority
- No Full GC events - heap sizing is appropriate
- Large GC pause detected (911ms) - investigate system-level delays

## Recommendations

- Continue profiling with longer-running tests to capture more application code
- Profile C++ native code directly to understand energy calculation bottlenecks

