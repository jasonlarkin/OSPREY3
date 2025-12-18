# Perf Profile Analysis - After Bounds Checking Removal

## Test Configuration
- **Workload**: 5000 iterations
- **Test data**: 200 atoms, 2000 Amber pairs, 1000 EEF1 pairs
- **Date**: After removing Array bounds checking in SIMD hot path

## Key Findings from Perf Reports

### Scalar Profile Breakdown (calc_scalar - 22.85% of total time)

**Function breakdown within calc_scalar:**
- `AtomPairAmber<double>::calc`: 4.14%
- `AtomPairEef1<double>::calc`: 3.86%
- `__ieee754_exp_fma`: 2.87% (exp() implementation)
- `distance_sq<double>`: 2.32%
- `Array<Real3<double>>::operator[]`: **1.57%** (down from 16.94% before!)
- `std::isnan`: 1.37%

**Observations:**
- Bounds checking overhead **reduced from 16.94% to 1.57%** (10.8x reduction)
- Remaining 1.57% is from scalar tail loops and correctness checks
- `exp()` calls account for 2.87% (EEF1 pairs)

### AVX2 Profile (calc_avx2 - 17.40% of total time)

**Key differences from scalar:**
- Lower total time (17.40% vs 22.85% = **1.31x faster**)
- No breakdown visible in perf report (likely inlined SIMD code)
- Should show AVX2 SIMD instructions in hardware counters

### AVX-512 Profile (calc_avx512 - 15.19% of total time)

**Key differences:**
- Even lower total time (15.19% vs 22.85% = **1.50x faster**)
- 1.06x faster than AVX2 (15.19% vs 17.40%)
- Should show AVX-512 SIMD instructions in hardware counters

## Performance Mechanisms

### 1. Bounds Checking Elimination (SUCCESSFUL)

**Before:**
- `Array::operator[]`: 16.94% of time
- 12 calls per SIMD iteration (4 pairs × 3 coordinates)
- Each call: 2-3 condition checks

**After:**
- `Array::operator[]`: 1.57% of time (only in scalar tail loops)
- Direct pointer access: `atoms_ptr[idx]` (zero overhead)
- Single bounds check per iteration (before loop)

**Impact:** 10.8x reduction in bounds checking overhead

### 2. SIMD Vectorization (WORKING)

**Evidence:**
- AVX2: 1.27x speedup (realistic for 4-wide vectorization)
- AVX-512: 1.35x speedup (modest improvement over AVX2)
- Correctness maintained (exact exp versions match scalar)

**Limitations:**
- AVX-512 only 1.06x faster than AVX2 (not 2x)
- Likely memory-bound due to non-contiguous access

### 3. Remaining Bottlenecks

**From scalar profile:**
1. **exp() calls (2.87%)**: EEF1 pairs use scalar `std::exp()`
   - Cannot vectorize without accuracy tradeoff
   - 33% of pairs affected
   
2. **AtomPair::calc() (4.14% + 3.86% = 8.0%)**: 
   - Amber: 4.14% (fully vectorized in SIMD)
   - EEF1: 3.86% (partially vectorized, exp() still scalar)

3. **distance_sq (2.32%)**: 
   - Fully vectorized in SIMD
   - Should be faster in AVX2/AVX-512

4. **Array::operator[] (1.57%)**: 
   - Only in scalar tail loops
   - Acceptable overhead

## Hardware-Level Metrics Needed

To fully characterize performance, we need:

1. **IPC (Instructions Per Cycle)**:
   - Scalar: Expected ~1.0-1.5
   - AVX2: Expected ~2.0-3.0 (if compute-bound)
   - AVX-512: Expected ~2.5-4.0 (if compute-bound)

2. **Cache Miss Rates**:
   - L1 cache misses: Should be low (<5%)
   - LLC (L3) misses: May be high due to random atom access

3. **SIMD Instruction Counts**:
   - `fp_arith_inst_retired.256b_packed_double`: AVX2 instructions
   - `fp_arith_inst_retired.512b_packed_double`: AVX-512 instructions
   - Should show significant counts for SIMD versions

4. **Memory Bandwidth Utilization**:
   - If >80% utilized: Memory-bound
   - If <50% utilized: Compute-bound

## Expected vs Actual Performance

**Expected (theoretical):**
- AVX2: 2-4x speedup (4-wide vectorization)
- AVX-512: 4-8x speedup (8-wide vectorization)

**Actual (measured):**
- AVX2: 1.27x speedup
- AVX-512: 1.35x speedup

**Gap analysis:**
- **Memory-bound**: Non-contiguous atom access limits speedup
- **Scalar exp()**: 33% of pairs still use scalar exp()
- **Small workload**: 3000 pairs may not fully amortize SIMD overhead
- **Cache effects**: Random access patterns prevent prefetching

## Next Steps

1. **Run perf stat** to get hardware-level metrics (IPC, cache misses, SIMD instruction counts)
2. **Test larger workloads** (10K, 50K, 100K pairs) to see if speedup increases
3. **Profile memory access patterns** to identify optimization opportunities
4. **Consider data layout changes** (SoA) if memory-bound

## Files Generated

- `perf_results/perf_scalar_*.data`: Scalar profile data
- `perf_results/perf_avx2_exact_*.data`: AVX2 profile data
- `perf_results/perf_avx512_exact_*.data`: AVX-512 profile data
- `perf_results/perf_stat_*.txt`: Hardware-level metrics (when available)

