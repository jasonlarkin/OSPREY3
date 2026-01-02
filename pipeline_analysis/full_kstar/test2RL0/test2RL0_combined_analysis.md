# Combined Profile Analysis: test2RL0

Generated: 2025-12-26 05:59:44

## Executive Summary

- **GC Overhead**: 0.51%
- **Total GC Events**: 33
- **Max GC Pause**: 370.57 ms
- **Peak Heap**: 0M

## GC Analysis

| Metric | Value |
|--------|-------|
| Total GC Events | 33 |
| Young GC | 25 |
| Full GC | 0 |
| Total Pause Time | 0.872 s |
| Average Pause | 26.41 ms |
| Maximum Pause | 370.57 ms |
| GC Overhead | 0.51% |
| Peak Heap | 0M |
| Total Duration | 169.58 s |

## Correlations and Insights

- GC overhead is very low (< 1%) - memory is not a bottleneck
- No Full GC events - heap sizing is appropriate
- Large GC pause detected (371ms) - investigate system-level delays

## Recommendations

- **Memory**: GC overhead is very low - focus optimization on CPU-bound operations

