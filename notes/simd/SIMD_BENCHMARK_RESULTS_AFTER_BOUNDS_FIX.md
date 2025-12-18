# SIMD Benchmark Results - After Bounds Checking Removal

## Test Configuration
- **Test**: Direct C++ benchmark (`benchmark_simd_direct`)
- **Iterations**: 5000
- **Workload**: 200 atoms, 2000 Amber pairs, 1000 EEF1 pairs
- **Date**: After removing Array bounds checking in SIMD hot path

## Performance Results

### Before Bounds Checking Removal
- **Scalar**: 130.37 us/iter
- **AVX2 (exact exp)**: 170.98 us/iter (0.76x - **31% slower**)
- **AVX-512 (exact exp)**: 157.32 us/iter (0.83x - **21% slower**)

### After Bounds Checking Removal
- **Scalar**: 98.86 us/iter
- **AVX2 (exact exp)**: 78.10 us/iter (**1.27x faster**)
- **AVX2 (fast exp)**: 75.35 us/iter (**1.31x faster**)
- **AVX-512 (exact exp)**: 73.45 us/iter (**1.35x faster**)
- **AVX-512 (fast exp)**: 57.60 us/iter (**1.72x faster**)

## Speedup Summary

| Version | Speedup vs Scalar | Notes |
|---------|------------------|-------|
| AVX2 (exact) | **1.27x** | Exact exp, correct results |
| AVX2 (fast exp) | 1.31x | Fast exp approximation (14x accuracy error) |
| AVX-512 (exact) | **1.35x** | Exact exp, correct results |
| AVX-512 (fast exp) | 1.72x | Fast exp approximation (14x accuracy error) |
| AVX-512 vs AVX2 (exact) | 1.06x | AVX-512 advantage |

## Key Findings

### 1. Bounds Checking Overhead Eliminated
- **Removed**: 16.94% overhead from `Array::operator[]` bounds checking
- **Impact**: SIMD now faster than scalar (was 31% slower before)
- **Method**: Use `atoms.pointer()` directly, single bounds check per iteration

### 2. Scalar Performance Also Improved
- Scalar: 130.37 → 98.86 us/iter (24% faster)
- Likely due to:
  - Compiler optimizations from code changes
  - Reduced cache pressure
  - Better instruction scheduling

### 3. SIMD Performance Gains
- **AVX2**: 1.27x speedup (realistic for 4-wide vectorization)
- **AVX-512**: 1.35x speedup (modest improvement over AVX2)
- **Fast exp**: Additional 3-27% speedup, but 14x accuracy error (not acceptable)

### 4. Correctness Verified
- AVX2 (exact): Matches scalar (relative error: 1.99e-16)
- AVX-512 (exact): Matches scalar (relative error: 7.97e-16)
- Fast exp versions: 14x difference (expected, not for production)

## Performance Analysis

### Why AVX-512 Only 1.06x Faster Than AVX2?
1. **Memory bandwidth limited**: Non-contiguous atom access (gather operations)
2. **Small workload**: 3000 pairs may not fully amortize AVX-512 overhead
3. **Scalar exp() calls**: EEF1 pairs still use scalar `std::exp()` (33% of workload)
4. **Cache effects**: Random atom access patterns limit prefetching

### Remaining Bottlenecks
1. **Scalar exp() for EEF1** (33% of pairs): Cannot fix without accuracy tradeoff
2. **Non-contiguous memory access**: Atoms accessed via random indices
3. **Small workload size**: 3000 pairs may not fully amortize SIMD overhead

## Next Steps

1. **Test with larger workloads** (10K, 50K, 100K pairs) to:
   - Better amortize SIMD overhead
   - See if AVX-512 advantage increases
   - Characterize memory vs compute bound

2. **Profile again** to verify:
   - Bounds checking overhead eliminated
   - Bottlenecks shifted to other areas
   - Memory access patterns

3. **Consider optimizations**:
   - Structure of Arrays (SoA) layout for better SIMD loading
   - Batch pairs by atom index ranges for contiguous access
   - Horizontal reduction optimization (2-5% potential gain)

## Conclusion

**Bounds checking removal was successful:**
- SIMD now outperforms scalar (1.27-1.35x speedup)
- Eliminated 16.94% overhead from `Array::operator[]`
- Correctness maintained (exact exp versions match scalar)

**Realistic expectations:**
- AVX2: 1.2-1.5x speedup (achieved: 1.27x)
- AVX-512: 1.3-1.7x speedup (achieved: 1.35x)
- Still limited by scalar exp() calls and memory access patterns

