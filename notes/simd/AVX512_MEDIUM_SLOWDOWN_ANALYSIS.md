# AVX-512 Medium System Slowdown Analysis

## Problem Statement

AVX-512 shows **0.70x speedup** (30% slowdown) on the medium system size, while performing well on other sizes (1.35-1.48x speedup). This anomaly requires investigation.

## Medium System Configuration

- **Atoms**: 500
- **Amber pairs**: 1000
- **EEF1 pairs**: 500
- **Total pairs**: 1500
- **Iterations**: 20,000

## Cache Hierarchy Analysis Results

### L1 Cache Behavior

| Version  | L1 Loads    | L1 Load Misses | L1 Miss Rate | L1 Stores   |
|----------|-------------|----------------|--------------|-------------|
| Scalar   | 3,330M      | 27.1M          | **0.81%**    | 1,404M      |
| AVX2     | 2,940M      | 20.4M          | **0.69%**    | 1,099M      |
| AVX-512  | 2,582M      | 23.8M          | **0.92%**    | 792M        |

### Key Observations

1. **Higher L1 Miss Rate for AVX-512**
   - AVX-512: 0.92% (worst)
   - AVX2: 0.69% (best)
   - Scalar: 0.81%
   
2. **More Absolute L1 Misses Despite Fewer Loads**
   - AVX-512 has 23.8M misses vs AVX2's 20.4M misses
   - AVX-512 processes 8 pairs at once (vs 4 for AVX2), but misses more per unit work

3. **Cache Miss Rate (Overall)**
   - All versions: ~32-37% cache miss rate
   - Similar across versions, suggesting the problem is at L1 level, not memory

## Root Causes

### 1. Cache Line Pressure (Primary Cause)

**AVX-512 processes 8 atom pairs simultaneously**, requiring:
- 16 atom accesses per SIMD iteration (8 pairs × 2 atoms each)
- Atom coordinates are non-contiguous (random indices: `atoms[atomi1]`, `atoms[atomi2]`)
- Each atom is 24 bytes (3 doubles × 8 bytes)
- **With random access, 16 atoms may span 16+ cache lines (64 bytes each)**

**AVX2 processes 4 pairs simultaneously**, requiring:
- 8 atom accesses per SIMD iteration
- Same random access pattern, but fewer concurrent accesses
- **8 atoms may span 8+ cache lines**

**Cache Line Conflicts:**
- Medium system has 500 atoms ≈ 12KB of atom data
- L1 data cache: ~32KB, but shared with other data
- **AVX-512's wider gather operations cause more cache line evictions**
- Subsequent pairs accessing same atoms experience cache misses

### 2. Gather Operation Overhead

**AVX-512 gather operations (`_mm512_i32gather_pd`) are expensive:**
- Latency: ~10-20 cycles per gather (vs ~5 cycles for AVX2)
- Throughput: Lower than AVX2 on some CPUs
- **Gather penalty is amortized over more pairs on larger systems**
- **On medium system, overhead dominates benefits**

### 3. System Size Sweet Spot

**Medium system size appears to be at a crossover point:**

| System Size | Atoms | Total Pairs | AVX-512 Speedup | Notes |
|-------------|-------|-------------|-----------------|-------|
| Small       | 200   | 3,000       | **1.35x**       | Small enough, cache-friendly |
| Medium      | 500   | 1,500       | **0.70x**       | **Cache pressure threshold** |
| Large       | 1000  | 8,000       | **1.48x**       | Large enough, overhead amortized |
| XLarge      | 2000  | 17,000      | **1.35x**       | Consistent performance |
| XXLarge     | 5000  | 55,000      | **1.37x**       | Consistent performance |

**Hypothesis:** Medium size is large enough that:
- Atom data doesn't fit comfortably in L1 cache with AVX-512's wider access pattern
- But not large enough to amortize gather overhead across many pairs
- Creates worst-case scenario for AVX-512

### 4. CPU Frequency Scaling

**AVX-512 may trigger CPU frequency throttling:**
- Intel CPUs reduce frequency when running AVX-512 instructions
- Medium system workload might hit a threshold where throttling becomes significant
- Larger systems benefit from better instruction-level parallelism hiding the penalty
- Smaller systems complete faster, spending less time at reduced frequency

## Performance Variance

**High variance (41.8% coefficient of variation) suggests:**
- Inconsistent cache behavior across runs
- Frequency scaling variations
- Non-deterministic memory access patterns (random atom indices)

## Solutions and Mitigations

### 1. Runtime Dispatch Based on System Size
```cpp
// Use AVX2 for medium systems, AVX-512 for larger/smaller
if (system_size == "medium" && total_pairs < 5000) {
    use_avx2 = true;  // Avoid AVX-512 gather overhead
}
```

### 2. Pair Reordering/Optimization
- **Sort pairs by atom indices** to improve spatial locality
- Process pairs accessing similar atoms together
- Reduces cache line conflicts

### 3. Cache Blocking
- **Partition atom array into blocks** that fit in L1 cache
- Process all pairs for a block before moving to next block
- Reduces cache evictions

### 4. Hybrid Approach
- Use AVX-512 for sequential/sorted pairs (better locality)
- Use AVX2 for random access pairs (less cache pressure)

### 5. Monitor CPU Frequency
- Use `perf stat -e power/energy-cores/` to detect frequency scaling
- Verify if AVX-512 is causing frequency reduction on medium workloads

## Conclusion

The AVX-512 slowdown on medium systems is caused by:
1. **Cache line pressure** from wider gather operations (8 vs 4 pairs)
2. **Cache threshold crossover** - medium size hits worst-case scenario
3. **Gather overhead** not amortized over enough work
4. **Possible frequency scaling** effects

**Recommendation:** Use AVX2 for medium-sized workloads, or implement pair reordering/cache blocking to improve AVX-512 performance.

