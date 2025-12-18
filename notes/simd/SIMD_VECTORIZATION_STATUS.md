# SIMD Vectorization Status

## Completed

### Amber Pairs (AVX2 & AVX-512)
- Fully vectorized distance calculation
- Fully vectorized energy computation (electrostatics + van der Waals)
- No scalar function calls in hot path

**Status**: Fully optimized

## Partially Complete

### EEF1 Pairs (AVX2)
- Vectorized distance calculation
- Vectorized parameter loading
- **Using exact scalar `std::exp()`** - extracting to array, calling scalar exp 4 times, reloading
- Cutoff mask handling

**Bottleneck**: Scalar `exp()` calls preserve accuracy but limit SIMD speedup

### EEF1 Pairs (AVX-512)
- Fully vectorized (distance, parameters, computation)
- **Using exact scalar `std::exp()`** - same as AVX2

## Performance Impact

**Current Benchmark Results** (2000 Amber + 1000 EEF1 pairs):
- Scalar: 106.05 us/iter
- AVX2: 148.87 us/iter (0.71x - **slower**)
- AVX-512: 128.50 us/iter (0.83x - **slower**)

**Analysis**:
- Amber pairs: ~67% of pairs, now fully vectorized → should show speedup
- EEF1 pairs: ~33% of pairs, partially vectorized → scalar `exp()` calls kill performance

## Root Cause

The scalar `exp()` function calls are expensive and break the SIMD pipeline:
```cpp
for (int j = 0; j < 4; j++) {
    exp_Xij2_array[j] = std::exp(neg_Xij2_array[j]);  // SCALAR CALL
    exp_Xji2_array[j] = std::exp(neg_Xji2_array[j]);  // SCALAR CALL
}
```

Each scalar `exp()` call:
- Forces pipeline flush
- Uses scalar FPU unit (not vectorized)
- Adds function call overhead
- Defeats SIMD optimization

## Accuracy Considerations

**CRITICAL**: Force field energy calculations require exact math for correctness. Fast exp approximations may:
- Affect energy gradients
- Change minimization convergence
- Alter protein folding simulations
- Cause numerical instabilities

**Current approach**: Using exact `std::exp()` maintains correctness at cost of SIMD performance.

## Versioning Strategy

Maintain separate implementations for testing:
1. **calc_scalar**: Original scalar implementation (baseline)
2. **calc_avx2**: AVX2 with exact exp (current)
3. **calc_avx512**: AVX-512 with exact exp (current)
4. **calc_avx2_fast_exp**: AVX2 with fast exp approximation (future, if needed)
5. **calc_avx512_fast_exp**: AVX-512 with fast exp approximation (future, if needed)

Each version must:
- Be correctness-verified against scalar
- Be benchmarked separately
- Document accuracy differences if any

## Next Steps

1. Verify correctness of current AVX2/AVX-512 implementations
2. Benchmark to measure actual performance vs scalar
3. Profile to identify remaining bottlenecks
4. Only implement fast exp if accuracy testing shows it's acceptable

