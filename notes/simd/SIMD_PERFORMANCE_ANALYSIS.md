# SIMD Performance Analysis

## Current Status

Benchmark results show **minimal to negative speedup** from SIMD optimizations:
- AVX2: Mean 0.676x speedup (actually ~1.48x faster when inverted, but inconsistent)
- AVX-512: Mean 0.703x speedup (actually ~1.42x faster when inverted, but inconsistent)
- Many test cases show SIMD **slower** than scalar

## Root Cause Identified

### Problem: Scalar Function Call Overhead

The current SIMD implementation only vectorizes the **distance calculation** (`r² = dx² + dy² + dz²`), but then **extracts to scalars** and calls `pair.calc()` for each pair:

```cpp
// Current AVX2 code (lines 106-112):
for (int j = 0; j < 4; j++) {
    double r2 = r2_array[j];  // Extract scalar
    double r = std::sqrt(r2);  // Scalar sqrt
    const AtomPairAmber<double>& pair = pair_amber[i+j];
    energy += pair.calc(r, r2, params.distance_dependent_dielectric);  // SCALAR CALL
}
```

### Profiling Evidence

From `perf report`:
- **16.94%** time in `Array::operator[]` (bounds checking)
- **11.66%** time in `AtomPairAmber::calc` (scalar function call)
- **10.36%** time in `AtomPairEef1::calc` (scalar function call)
- Only **7.01%** time in actual math (`__ieee754_exp_fma`)

**Total overhead: ~22% from scalar function calls alone**

### Roofline Plot Analysis

The roofline plot shows all implementations at **10⁻¹² GFLOP/s**, ~12 orders of magnitude below theoretical peak. This confirms:
1. Function call overhead dominates
2. SIMD instructions are not being effectively utilized
3. Memory access patterns may be inefficient

## What Needs to Be Vectorized

### AtomPairAmber::calc() Computation

```cpp
// Current scalar implementation:
T calc(T r, T r2, bool distance_dependent_dielectric) const {
    T es;
    if (distance_dependent_dielectric) {
        es = esQ / r2;
    } else {
        es = esQ / r;  // Uses scalar 1/r = 1/sqrt(r2)
    }
    
    T r6 = r2 * r2 * r2;  // r²³
    T r12 = r6 * r6;      // r⁶²
    T vdw = vdwA / r12 - vdwB / r6;
    
    return es + vdw;
}
```

**To vectorize:**
- Load `esQ`, `vdwA`, `vdwB` for 4/8 pairs into vectors
- Calculate `1/r²` and `1/r` using `_mm256_rsqrt_pd` or `_mm512_rsqrt14_pd`
- Vectorize: `r6 = r2 * r2 * r2` → `_mm256_mul_pd` chains
- Vectorize: `r12 = r6 * r6` → `_mm256_mul_pd`
- Vectorize: `vdw = vdwA/r12 - vdwB/r6` → `_mm256_fmsub_pd`
- Horizontal sum at end

### AtomPairEef1::calc() Computation

```cpp
// Current scalar implementation:
T calc(T r, T r2) const {
    if (r > 9.0) return 0.0;  // Early exit - problematic for SIMD
    
    T Xij = (r - vdwRadius1) / lambda1;
    T Xji = (r - vdwRadius2) / lambda2;
    return -(alpha1 * exp(-Xij*Xij) + alpha2 * exp(-Xji*Xji)) / r2;
}
```

**To vectorize:**
- Load `vdwRadius1`, `lambda1`, `alpha1`, `vdwRadius2`, `lambda2`, `alpha2` for 4/8 pairs
- Vectorize: `Xij = (r - vdwRadius1) / lambda1` → `_mm256_sub_pd`, `_mm256_div_pd`
- Vectorize: `exp(-Xij*Xij)` → Use `_mm256_exp_pd` (or approximate with polynomial)
- Vectorize: `alpha1*exp1 + alpha2*exp2` → `_mm256_fmadd_pd`
- Handle early exit: Use masks for `r > 9.0` condition

**Challenge:** `exp()` is expensive and may need approximation or lookup table for SIMD.

## Performance Expectations After Fix

Once the energy computation is fully vectorized:

**Expected speedups:**
- AVX2: **2-4x** (8 doubles processed in 2 cycles of 4)
- AVX-512: **4-8x** (8 doubles processed in 1 cycle)

**But realistic expectations:**
- Memory bandwidth may limit to 2-3x
- `exp()` is expensive, may limit EEF1 pairs to 1.5-2x
- Overall: **1.5-3x** speedup is realistic

## Next Steps

1. **Vectorize AtomPairAmber::calc()**
   - Start with AVX2 (simpler, better compiler support)
   - Use `_mm256_rsqrt_pd` for `1/r`
   - Use FMA instructions (`_mm256_fmadd_pd`, `_mm256_fmsub_pd`)

2. **Vectorize AtomPairEef1::calc()**
   - Handle `r > 9.0` early exit with masks
   - Implement fast `exp()` approximation or use library
   - Consider pre-computing `1/r2` in SIMD

3. **Optimize memory access**
   - Ensure pair data is aligned
   - Consider SoA (Structure of Arrays) layout for better SIMD loading
   - Minimize `Array::operator[]` bounds checking overhead

4. **Re-benchmark**
   - Run parameter sweep again
   - Verify roofline plot shows improvement
   - Check `perf` profile shows SIMD instructions in hot path

## Files to Modify

- `src/main/cc/ConfEcalc/energy_ambereef1_simd.h` - Add vectorized energy computation
- Potentially refactor `AtomPairAmber` and `AtomPairEef1` data layout for better SIMD access

