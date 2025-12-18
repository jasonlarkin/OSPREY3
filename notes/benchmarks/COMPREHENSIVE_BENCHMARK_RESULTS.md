# Comprehensive SIMD Benchmark Results

## Test Configuration
- **Runs per config**: 3
- **System sizes**: 5 sizes (small → xxlarge)
- **Benchmarks**: Separate executables for scalar, AVX2, AVX-512

**Plots generated**: All visualizations are available in the `plots/` directory.

## Performance Summary

### Mean Time per Iteration (microseconds)

![Performance Comparison](plots/performance_comparison.png)

| Version | System  | Mean (us) | Median (us) | StdDev (us) | Min (us) | Max (us) |
|---------|---------|-----------|-------------|-------------|----------|----------|
| scalar  | small   | 100.24    | 99.86       | 4.55        | 95.89    | 104.98   |
| avx2    | small   | 87.75     | 86.63       | 6.46        | 81.93    | 94.69    |
| avx512  | small   | 74.52     | 74.53       | 0.08        | 74.44    | 74.60    |
| scalar  | medium  | 52.77     | 48.66       | 7.26        | 48.49    | 61.15    |
| avx2    | medium  | 43.80     | 45.75       | 3.83        | 39.39    | 46.26    |
| avx512  | medium  | 75.09     | 87.43       | 31.42       | 39.38    | 98.47    |
| scalar  | large   | 208.31    | 208.01      | 1.91        | 206.57   | 210.36   |
| avx2    | large   | 180.66    | 173.66      | 13.53       | 172.07   | 196.26   |
| avx512  | large   | 140.61    | 138.92      | 3.18        | 138.63   | 144.27   |
| scalar  | xlarge  | 438.32    | 426.32      | 22.97       | 423.84   | 464.80   |
| avx2    | xlarge  | 377.81    | 354.09      | 41.74       | 353.33   | 426.01   |
| avx512  | xlarge  | 324.09    | 313.19      | 19.10       | 312.92   | 346.14   |
| scalar  | xxlarge | 1371.86   | 1363.70     | 16.42       | 1361.11  | 1390.76  |
| avx2    | xxlarge | 1131.20   | 1131.96     | 8.61        | 1122.23  | 1139.40  |
| avx512  | xxlarge | 1002.12   | 1001.04     | 3.92        | 998.85   | 1006.47  |

## Speedup vs Scalar

![Speedup Comparison](plots/speedup_comparison.png)

| System  | AVX2 Speedup | AVX-512 Speedup | Best Version |
|---------|--------------|-----------------|--------------|
| small   | 1.142x       | **1.345x**      | AVX-512      |
| medium  | 1.205x       | 0.703x          | AVX2         |
| large   | 1.153x       | **1.482x**      | AVX-512      |
| xlarge  | 1.160x       | **1.352x**      | AVX-512      |
| xxlarge | 1.213x       | **1.369x**      | AVX-512      |

![Speedup Trend](plots/speedup_trend.png)

## Key Findings

### 1. AVX-512 Performance on Large Workloads

**AVX-512 is fastest on 4 out of 5 system sizes:**
- **Large (1,000 atoms, 8K pairs)**: 1.482x speedup (best overall)
- **XLarge (2,000 atoms, 17K pairs)**: 1.352x speedup
- **XXLarge (5,000 atoms, 55K pairs)**: 1.369x speedup
- **Small (200 atoms, 3K pairs)**: 1.345x speedup

**Conclusion**: AVX-512 performs best on larger workloads where the overhead is amortized.

### 2. Medium System Anomaly

**Medium system (500 atoms, 1.5K pairs) shows AVX-512 slowdown:**
- AVX-512: 0.703x (slower than scalar)
- High variance: StdDev 31.42 (vs 0.08-19 for other sizes)
- One run showed 39.38 us (fast), others showed 87-98 us (slow)

**Possible causes:**
- CPU frequency scaling triggered inconsistently
- Workload size hits a "sweet spot" that causes AVX-512 penalties
- Random variation due to small workload size
- Need more runs to determine if this is statistical noise

### 3. AVX2 Consistency

**AVX2 shows consistent ~1.15-1.21x speedup across all sizes:**
- More stable than AVX-512
- No frequency scaling issues
- Reliable performance gain

### 4. Variance Analysis

![Variance Analysis](plots/variance_analysis.png)

**Lowest variance (most stable):**
- AVX-512 small: StdDev 0.08 (very consistent, CoV 0.1%)
- AVX-512 large: StdDev 3.18
- Scalar large: StdDev 1.91

**Highest variance:**
- AVX-512 medium: StdDev 31.42 (inconsistent, CoV 41.8%)
- AVX2 xlarge: StdDev 41.74

The variance analysis plot shows the Coefficient of Variation (CV = stddev/mean × 100%) for each version across system sizes. AVX-512 on medium systems shows extremely high variance (41.8% CV), indicating inconsistent performance that may be due to CPU frequency scaling or other system-level effects.

## System Size Details

| System  | Atoms | Amber Pairs | EEF1 Pairs | Total Pairs | Iterations |
|---------|-------|-------------|------------|-------------|------------|
| small   | 200   | 2,000       | 1,000      | 3,000       | 50,000     |
| medium  | 500   | 1,000       | 500        | 1,500       | 20,000     |
| large   | 1,000 | 7,500       | 500        | 8,000       | 10,000     |
| xlarge  | 2,000 | 15,000      | 2,000      | 17,000      | 5,000      |
| xxlarge | 5,000 | 50,000      | 5,000      | 55,000      | 1,000      |

## Conclusions

1. **AVX-512 is effective on larger workloads** (1.35-1.48x speedup)
2. **AVX2 provides consistent moderate speedup** (1.15-1.21x) across all sizes
3. **Medium system needs investigation** - may require more runs or different workload
4. **Separate benchmarking is essential** - previous combined benchmarks were misleading
5. **AVX-512 variance is low on large systems** - very consistent performance

## Recommendations

1. **Use AVX-512 for large workloads** (>8K pairs)
2. **Use AVX2 for medium workloads** or when AVX-512 shows variance
3. **Run more iterations on medium system** to determine if slowdown is real
4. **Profile medium system** to understand AVX-512 variance
5. **Consider runtime dispatch** that switches based on workload size

## Next Steps

1. Re-run medium system with more iterations (10 runs)
2. Profile medium system with `perf stat` to check CPU frequency
3. Test with actual OSPREY workloads (2RL0: 16,734 pairs)
4. Measure memory bandwidth utilization
5. Analyze cache behavior differences between sizes

