# Memory-Bound vs Compute-Bound Analysis

## Why Pair Computation is Memory-Bound

### Arithmetic Intensity Calculation

The roofline model determines if a workload is **memory-bound** or **compute-bound** based on:

```
Compute/Memory Boundary = CPU_Peak_FLOPs / Memory_Bandwidth
```

For our CPU (Intel i3-1115G4):
- **Peak Compute**: ~100 GFLOP/s
- **Memory Bandwidth**: ~50 GB/s
- **Boundary**: 100 / 50 = **2.0 FLOPs/byte**

**Rule:**
- If AI > 2.0 FLOPs/byte → **Compute-bound** (limited by CPU)
- If AI < 2.0 FLOPs/byte → **Memory-bound** (limited by memory bandwidth)

### Pair Computation Results

| Version | Arithmetic Intensity | GFLOP/s | Bound |
|---------|----------------------|---------|-------|
| Scalar  | 0.003 FLOPs/byte     | 14.3    | Memory |
| AVX2    | 0.004 FLOPs/byte     | 26.0    | Memory |
| AVX-512 | 0.005 FLOPs/byte     | 31.9    | Memory |

**All versions are 400-600x BELOW the compute/memory boundary (2.0), confirming memory-bound behavior.**

### Why Pair Computation is Memory-Bound

1. **Non-contiguous Memory Access**
   - Each pair accesses two atoms: `atoms[atomi1]` and `atoms[atomi2]`
   - Atom indices are random (not sequential), causing cache misses
   - SIMD gather operations are expensive (AVX-512 gather latency: ~10-20 cycles)

2. **Low Computation per Memory Access**
   - Per pair: ~28 FLOPs (distance, sqrt, electrostatics, VdW)
   - Per pair: ~128 bytes memory traffic (2 atoms × 3 coords × 8 bytes + parameters)
   - AI = 28 / 128 = **0.22 FLOPs/byte** (theoretical)
   - Measured: **0.003-0.005 FLOPs/byte** (actual, accounting for cache effects)

3. **Cache Line Effects**
   - Each atom access may span multiple cache lines (64 bytes)
   - Non-contiguous access pattern prevents prefetching
   - Cache misses force main memory access

4. **Limited Data Reuse**
   - Each atom is accessed once per pair
   - No temporal locality (atoms not reused across pairs)
   - Spatial locality is poor (random atom indices)

### Compute-Bound Benchmark Comparison

Matrix multiplication (compute-bound benchmark):
- **AI = 42.67 FLOPs/byte** (21x above boundary)
- **Access pattern**: Sequential, cache-friendly
- **Data reuse**: High (each element used multiple times)
- **Performance**: Limited by CPU compute, not memory

### Implications for Optimization

Since pair computation is **memory-bound**:

1. **SIMD helps by:**
   - Reducing memory traffic per FLOP (vectorized operations)
   - Better cache utilization (processing multiple pairs)
   - AVX-512: 23% less memory reads, 44% less writes vs scalar

2. **Further optimizations should focus on:**
   - **Memory access patterns**: Reorder pairs to improve locality
   - **Cache blocking**: Process pairs in blocks that fit in cache
   - **Prefetching**: Explicit prefetch hints for next atom pairs
   - **Data layout**: Structure-of-arrays (SoA) instead of array-of-structures (AoS)

3. **SIMD speedup is limited by:**
   - Memory bandwidth (not CPU compute)
   - Current speedup: 1.35x (AVX-512) is good for memory-bound workload
   - Theoretical max speedup ≈ memory bandwidth improvement

### Summary

- **Pair computation is memory-bound** (AI = 0.003-0.005 << 2.0)
- **SIMD provides 1.35x speedup** by reducing memory traffic
- **Further optimization requires memory access pattern improvements**
- **Compute-bound benchmark (matrix multiply) demonstrates AI = 42.67** for comparison

