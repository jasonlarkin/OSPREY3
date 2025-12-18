# SIMD Implementation Versioning Strategy

## Purpose

Maintain separate, testable implementations to verify:
- Correctness (exact match with scalar)
- Performance (benchmark each version)
- Accuracy (if using approximations)

## Current Versions

### calc_scalar()
- Original scalar implementation
- Baseline for correctness and performance
- Always available, no SIMD dependencies

### calc_avx2()
- AVX2 vectorized implementation
- Uses exact `std::exp()` for EEF1 pairs (preserves accuracy)
- Amber pairs fully vectorized
- EEF1 pairs: distance vectorized, exp() scalar

### calc_avx512()
- AVX-512 vectorized implementation  
- Uses exact `std::exp()` for EEF1 pairs (preserves accuracy)
- Amber pairs fully vectorized
- EEF1 pairs: distance vectorized, exp() scalar

## Future Versions (if needed)

### calc_avx2_fast_exp()
- AVX2 with fast exp approximation for EEF1
- Must be correctness-verified against calc_scalar()
- Requires accuracy analysis for force field impact

### calc_avx512_fast_exp()
- AVX-512 with fast exp approximation for EEF1
- Must be correctness-verified against calc_scalar()
- Requires accuracy analysis for force field impact

## Testing Requirements

Each version must:
1. **Correctness**: Produce identical results to `calc_scalar()` (within floating-point epsilon)
2. **Performance**: Benchmark separately, document speedup/slowdown
3. **Accuracy** (for approximations): Verify force field behavior unchanged:
   - Energy gradients
   - Minimization convergence
   - Protein folding simulations

## Implementation Pattern

```cpp
// Scalar baseline
inline double calc_scalar(...) { /* original code */ }

// AVX2 with exact exp
inline double calc_avx2(...) { /* current implementation */ }

// AVX2 with fast exp (future)
#ifdef ENABLE_FAST_EXP
inline double calc_avx2_fast_exp(...) { /* approximate exp */ }
#endif
```

## Dispatch Logic

Runtime dispatch selects best available version:
1. Check CPU capabilities
2. Select: AVX-512 > AVX2 > Scalar
3. Future: Optionally enable fast_exp versions via environment variable

## Files

- `energy_ambereef1_simd.h`: Contains all SIMD implementations
- `energy_ambereef1.h`: Dispatch logic and scalar fallback
- Benchmarks: Test all versions separately

