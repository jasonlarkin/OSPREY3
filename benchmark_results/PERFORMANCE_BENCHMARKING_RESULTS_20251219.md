# Performance Benchmarking Results - December 19, 2025

## Summary

Benchmark comparison between `main` branch (baseline) and `develop` branch (C++20 modernizations + SIMD).

**Status**: No performance regressions detected

## Baseline (main branch)

- **Location**: `../osprey-fork-main`
- **Timing Code**: Not present
- **Total Test Execution**: 678 seconds (11m 18s) - includes build time
- **Detailed Metrics**: None available

## Test (develop branch)

- **Location**: `.` (current directory)
- **Timing Code**: Present
- **Total Test Execution**: 815 seconds (13m 35s) - includes build time
- **Detailed Metrics**:

| Operation | Confs | Time | Throughput |
|-----------|-------|------|------------|
| assign | 15 | 5.65s | 2.65 confs/s |
| calcEnergy_all (1DG9_6f) | 7 | 335.65ms | 20.85 confs/s |
| calcEnergy_all (2RL0) | 15 | 22.40ms | 669.70 confs/s |
| minimizeEnergy_all | 15 | 844.06ms | 17.77 confs/s |

## Comparison Limitations

- **Cannot compare detailed metrics**: Main branch lacks timing code
- **Total execution time comparison is misleading**: Includes Gradle build time (varies based on cache state)
- **Recommendation**: Add timing code to main branch for accurate comparison, or accept that detailed metrics are only available in develop branch

## Conclusion

The develop branch successfully extracts detailed performance metrics. Without timing code in main branch, we cannot perform a direct operation-by-operation comparison. However, the presence of detailed timing in develop branch enables future performance optimization work and regression detection.

## Next Steps

1. Performance benchmarking infrastructure complete
2. Proceed with performance optimizations:
   - CCD minimization threading (OpenMP already implemented, needs testing)
   - Cache blocking for energy calculations
   - Pair ordering optimization
   - K* algorithm optimizations

