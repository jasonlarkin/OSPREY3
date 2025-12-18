# SIMD Performance Bottleneck Analysis

## Current Performance: SIMD is Slower Than Scalar

**Benchmark Results**:
- Scalar: 130.37 us/iter
- AVX2 (exact exp): 170.98 us/iter (0.76x - **31% slower**)
- AVX-512 (exact exp): 157.32 us/iter (0.83x - **21% slower**)

## Root Causes from Profiling

### 1. Scalar exp() Pipeline Break (33% of pairs)

**Problem**: EEF1 pairs (33% of workload) still use scalar `std::exp()` calls:

```cpp
// Lines 279-282 in energy_ambereef1_simd.h
for (int j = 0; j < 4; j++) {
    exp_Xij2_array[j] = std::exp(neg_Xij2_array[j]);  // SCALAR CALL
    exp_Xji2_array[j] = std::exp(neg_Xji2_array[j]);  // SCALAR CALL
}
```

**Impact**:
- Forces pipeline flush every 4 pairs
- Uses scalar FPU unit (not vectorized)
- Function call overhead
- Defeats SIMD optimization for entire EEF1 section

**Evidence from perf**: 10.36% time in `AtomPairEef1::calc` (even after vectorizing distance)

### 2. Array Bounds Checking Overhead (17% of time)

**Problem**: Every `atoms[idx]` access calls `Array::operator[]` with assertions:

```cpp
// array.h lines 27-32
inline T & operator [] (int64_t i) {
    assert (i >= 0);      // Bounds check #1
    assert (i < size);    // Bounds check #2
    return pointer()[i];   // May check nullptr
}
```

**In SIMD loop**: Loading 4 atom coordinates requires 12 `operator[]` calls:
- `atoms[idx1[3]].x, atoms[idx1[2]].x, atoms[idx1[1]].x, atoms[idx1[0]].x` (4 calls)
- Same for y, z (8 more calls)
- Total: 12 calls per loop iteration
- Each call: 2-3 condition checks

**Evidence from perf**: 16.94% time in `Array::operator[]`

**Solution**: Use `atoms.pointer()` directly and manually bounds-check once, then use raw pointer arithmetic.

### 3. Non-Contiguous Memory Access (Gather Overhead)

**Problem**: Atoms are indexed non-sequentially:
```cpp
atoms[idx1[3]].x  // idx1[3] = random atom index
atoms[idx1[2]].x  // idx1[2] = different random atom index
// etc.
```

**Impact**:
- Cache misses: atoms accessed in random order
- No prefetching benefit
- Gather operations are slower than contiguous loads
- AVX2 doesn't have efficient gather for doubles (`_mm256_i32gather_pd` exists but is slow)

**Better approach**: Structure of Arrays (SoA) layout, or batch processing by atom index ranges.

### 4. Small Workload Size

**Current test**: 2000 Amber + 1000 EEF1 = 3000 pairs total

**SIMD overhead per iteration**:
- Loop setup: ~10-20 cycles
- Parameter loading: ~20-30 cycles
- Horizontal reduction: ~10-15 cycles
- Total overhead: ~40-65 cycles per 4/8 pairs

**Amortization**: Overhead only justified for >100 pairs processed. With 3000 pairs:
- AVX2: 3000/4 = 750 iterations, overhead = 750 * 50 cycles = 37,500 cycles
- Scalar: 3000 iterations, minimal overhead per iteration

**Impact**: Small workload means setup overhead dominates.

### 5. Memory Bandwidth Bottleneck

**Compute intensity**: ~0.45 FLOPs/Byte (from benchmark)

**Memory access per pair**:
- Read 2 atoms (6 doubles = 48 bytes)
- Read pair parameters (3-7 doubles = 24-56 bytes)
- Total: ~72-104 bytes per pair
- Operations: ~25 FLOPs per pair

**Bandwidth calculation**:
- DDR4: ~50 GB/s theoretical, ~30 GB/s practical
- At 3000 pairs/130us = 23M pairs/sec
- Memory: 23M * 72 bytes = 1.66 GB/s
- Utilization: 1.66/30 = 5.5% of bandwidth

**Conclusion**: Not memory-bound, but memory access patterns (non-contiguous, scattered) reduce effective bandwidth.

## Why Scalar is Faster

Scalar version benefits from:
1. **Better compiler optimizations**: Compiler can optimize scalar loops better (inlining, loop unrolling)
2. **Fewer function calls**: Direct access patterns, no SIMD setup
3. **Better cache utilization**: Sequential access to pairs array
4. **No pipeline breaks**: Consistent execution flow
5. **Smaller code size**: Better instruction cache usage

## Performance Opportunities

### High Impact Fixes

1. **Eliminate bounds checking in SIMD hot path**
   - Use `atoms.pointer()` directly
   - Manual bounds check once before loop
   - Use raw pointer arithmetic
   - Expected gain: 10-15%

2. **Use contiguous memory loads where possible**
   - Batch pairs by atom index ranges
   - Or: Structure of Arrays layout for atoms
   - Expected gain: 5-10%

3. **Larger test workloads**
   - Test with 10K+ pairs to amortize SIMD overhead
   - Expected: Show speedup once overhead is negligible

### Medium Impact

4. **Optimize horizontal reduction**
   - Use `_mm256_hadd_pd` or shuffle instead of store+sum
   - Expected gain: 2-5%

5. **Reduce function call overhead**
   - Inline more aggressively
   - Template specializations for common cases

### Low Impact (Already Done)

6. ~~Vectorize Amber pairs~~ - DONE
7. ~~Vectorize distance calculation~~ - DONE

## Expected Performance After Fixes

**With bounds checking removal + larger workload**:
- AVX2: 1.2-1.5x speedup (realistic)
- AVX-512: 1.3-1.7x speedup (realistic)

**Still limited by**:
- Scalar exp() calls for EEF1 pairs (cannot be fixed without accuracy tradeoff)
- Non-contiguous memory access (requires data layout changes)
- Memory bandwidth (if workload increases significantly)

## Test Strategy

1. **Remove bounds checking**: Re-run benchmark
2. **Increase workload**: Test with 10K, 50K, 100K pairs
3. **Profile again**: Verify bottlenecks shift
4. **Measure**: Actual speedup vs scalar

## Files to Modify

1. `energy_ambereef1_simd.h`: Use `atoms.pointer()` directly, remove `operator[]` calls
2. Benchmark: Test with larger workloads

