# Why AVX-512 is Slower Than Scalar: Analysis

## Current Results
- **Scalar**: 122.48 us/iter
- **AVX2**: 93.48 us/iter (1.36x faster)
- **AVX-512**: 136.46 us/iter (0.90x - SLOWER than scalar)

## Data Size Analysis

**Atoms array size:**
- 200 atoms × 3 coordinates × 8 bytes = **4.8 KB**
- L1 cache: typically 32 KB (data) + 32 KB (instruction)
- L2 cache: typically 256 KB - 1 MB
- **Conclusion: All atoms easily fit in L1 cache**

**Pairs data size:**
- 2000 Amber pairs × 32 bytes = 64 KB
- 1000 EEF1 pairs × 48 bytes = 48 KB
- Total: ~112 KB (fits in L2, may spill to L3)

**Memory-bound claim is WRONG for atoms array.**
The atoms themselves are tiny and should be in L1 cache.

## Why AVX-512 is Slower

### 1. CPU Frequency Scaling
- AVX-512 instructions may trigger CPU frequency downclocking
- Intel CPUs often reduce frequency when AVX-512 is used to manage power/thermal limits
- This can negate any instruction-level speedup

### 2. Gather Operation Overhead
Even though atoms are in L1 cache, the gather pattern is inefficient:

**Current code pattern:**
```cpp
// Load 8 atom1 coordinates (non-contiguous)
__m512d x1 = _mm512_set_pd(
    atoms_ptr[idx1[7]].x, atoms_ptr[idx1[6]].x, 
    atoms_ptr[idx1[5]].x, atoms_ptr[idx1[4]].x,
    atoms_ptr[idx1[3]].x, atoms_ptr[idx1[2]].x,
    atoms_ptr[idx1[1]].x, atoms_ptr[idx1[0]].x
);
```

**Problem:**
- `_mm512_set_pd` compiles to 8 individual load instructions
- Even if all 8 atoms are in L1 cache, this requires 8 separate load operations
- No prefetching benefit (random access pattern)
- Cache line conflicts possible if indices map to same cache line

**Better approach (if indices were contiguous):**
```cpp
// If indices were sequential, could use:
__m512d x1 = _mm512_loadu_pd(&atoms_ptr[idx1[0]].x);  // Single instruction
```

### 3. Cache Line Conflicts
- 8 random atom accesses per iteration
- If multiple indices map to same 64-byte cache line, conflicts occur
- L1 cache associativity limits (typically 8-way)
- More conflicts with 8-wide access vs 4-wide (AVX2)

### 4. Instruction Overhead
- AVX-512 instructions have higher latency than AVX2
- More register pressure (32 zmm registers vs 16 ymm)
- Register spills to stack if not enough registers

### 5. Memory Access Pattern
**Scalar pattern:**
```
for each pair:
  load atom1 (sequential access through pairs array)
  load atom2 (sequential access through pairs array)
  compute
```
- Pairs array is accessed sequentially (good prefetching)
- Atom access is random but only 2 loads per iteration
- Better cache utilization

**AVX-512 pattern:**
```
for 8 pairs at once:
  load 8 atom1 indices (sequential from pairs array)
  load 8 atom2 indices (sequential from pairs array)
  load 8 atom1 coordinates (RANDOM from atoms array)
  load 8 atom2 coordinates (RANDOM from atoms array)
  compute (vectorized)
```
- 16 random loads per iteration (8 atom1 + 8 atom2)
- No prefetching benefit for random atom access
- Cache line conflicts more likely

## Why AVX2 Works Better

1. **4-wide is sweet spot**: Fewer cache conflicts, better register utilization
2. **Less frequency scaling**: AVX2 doesn't trigger as aggressive downclocking
3. **Better instruction throughput**: AVX2 instructions have lower latency
4. **Fewer random loads**: 8 loads per iteration vs 16 for AVX-512

## Root Cause

**The slowdown is NOT due to memory bandwidth or cache size.**
- Atoms fit easily in L1 cache (4.8 KB)
- The issue is **cache line conflicts** and **gather operation overhead**

**Even with data in L1 cache:**
- 8 random loads still require 8 separate load instructions
- Cache line conflicts reduce effective bandwidth
- CPU frequency scaling reduces clock speed

## Potential Solutions

1. **Sort pairs by atom index**: Group pairs that access similar atoms
   - Reduces cache line conflicts
   - Enables better prefetching
   - May enable contiguous loads for some pairs

2. **Use AVX-512 gather instructions**: `_mm512_i64gather_pd`
   - Single instruction instead of 8 loads
   - May have better cache behavior
   - Still has latency but better throughput

3. **Hybrid approach**: Use AVX2 for random access, AVX-512 for contiguous access
   - Detect when indices are sequential
   - Use AVX-512 gather for random, AVX-512 load for sequential

4. **Disable AVX-512 frequency scaling**: If CPU supports it
   - May require BIOS/UEFI settings
   - Not always possible

5. **Stick with AVX2**: It's already faster than scalar
   - 1.36x speedup is good
   - AVX-512 may not be worth the complexity for this workload

## Next Steps

1. Profile with `perf stat` on separate runs to confirm frequency scaling
2. Check CPU frequency during AVX-512 execution: `watch -n 0.1 grep MHz /proc/cpuinfo`
3. Test AVX-512 gather instructions vs current approach
4. Measure cache line conflicts: `perf c2c record`

