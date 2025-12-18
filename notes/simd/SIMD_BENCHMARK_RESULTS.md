# SIMD Benchmark Results

## All Versions Compiled

All 5 implementations are now compiled and available for testing:
1. `calc_scalar` - Original scalar implementation
2. `calc_avx2` - AVX2 with exact `std::exp()`
3. `calc_avx2_fast_exp` - AVX2 with fast exp approximation (Taylor series)
4. `calc_avx512` - AVX-512 with exact `std::exp()`
5. `calc_avx512_fast_exp` - AVX-512 with fast exp approximation

## Correctness Test Results

**Test Configuration**: 200 atoms, 2000 Amber pairs, 1000 EEF1 pairs

| Version | Energy | Difference from Scalar | Relative Error |
|---------|--------|------------------------|----------------|
| Scalar (baseline) | 73074.6643789626 | 0.0 | 0.0 |
| AVX2 (exact exp) | 73074.6643789626 | 0.0 | 1.99e-16 |
| AVX-512 (exact exp) | 73074.6643789626 | 0.0000000001 | 7.97e-16 |
| AVX2 (fast exp) | 1105671.7201021826 | 1032597.06 | **14.1x error** |
| AVX-512 (fast exp) | 1105671.7201021826 | 1032597.06 | **14.1x error** |

**Conclusion**: Fast exp approximation is unusable for force field calculations. Exact exp versions maintain correct results.

## Performance Test Results

**Benchmark Configuration**: 5000 iterations, 200 atoms, 2000 Amber pairs, 1000 EEF1 pairs

| Version | Time (us/iter) | Speedup vs Scalar |
|---------|----------------|-------------------|
| Scalar | 130.37 | 1.00x (baseline) |
| AVX2 (exact exp) | 170.98 | **0.76x (slower)** |
| AVX-512 (exact exp) | 157.32 | **0.83x (slower)** |

Fast exp versions not yet benchmarked (accuracy too poor to be useful).

## Analysis

### Why SIMD is Slower

1. **Scalar exp() calls**: Even though Amber pairs are fully vectorized, EEF1 pairs still use scalar `std::exp()` calls, breaking the SIMD pipeline
2. **Memory access overhead**: Array bounds checking and non-contiguous access patterns
3. **Small workload**: 3000 total pairs may not be enough to amortize SIMD setup costs
4. **Function call overhead**: Dispatch and setup overhead

### Fast Exp Approximation

Taylor series approximation (5 terms: 1 + x + x²/2 + x³/6 + x⁴/24 + x⁵/120) produces **14x error**:
- Completely unusable for force field calculations
- Would break energy minimization
- Would corrupt protein folding simulations
- Confirms need for exact math in molecular simulations

## Next Steps

1. Profile with `perf` to identify exact bottlenecks
2. Optimize memory access patterns (reduce bounds checking)
3. Test with larger workloads (more pairs)
4. Consider structure-of-arrays (SoA) layout for better SIMD loading
5. Fast exp approximation is **not viable** - abandon or use higher-order approximation (with accuracy testing)

## Environment Variables

- `OSPREY_FORCE_SCALAR=1` - Force scalar execution
- `OSPREY_USE_FAST_EXP=1` - Use fast exp approximation (not recommended due to accuracy)

