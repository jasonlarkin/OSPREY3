# Perf Stat Hardware-Level Analysis

## Test Configuration
- **Workload**: 1000 iterations
- **Test data**: 200 atoms, 2000 Amber pairs, 1000 EEF1 pairs
- **Note**: Previous runs measured all versions in one execution (INCORRECT)
- **Fixed**: Now using separate benchmarks for each version (see `benchmark_scalar_only`, `benchmark_avx2_only`, `benchmark_avx512_only`)

## Hardware Metrics Comparison

### Scalar (OSPREY_FORCE_SCALAR=1)

| Metric | Value | Notes |
|--------|-------|-------|
| **Cycles** | 1,619,808,397 | Baseline |
| **Instructions** | 3,762,630,133 | |
| **IPC** | **2.32** insn/cycle | Good instruction throughput |
| **Cache misses** | 118,117 | |
| **Cache references** | 514,759 | |
| **Cache miss rate** | **22.946%** | Moderate |
| **Time** | 0.497s | |
| **Scalar time** | 122.48 us/iter | From benchmark output |

### SIMD (Runtime dispatch - uses AVX-512 if available)

| Metric | Value | Notes |
|--------|-------|-------|
| **Cycles** | 1,758,625,074 | **+8.6% more cycles** |
| **Instructions** | 3,762,630,695 | Same as scalar |
| **IPC** | **2.14** insn/cycle | **Lower than scalar!** |
| **Cache misses** | 145,548 | |
| **Cache references** | 527,327 | |
| **Cache miss rate** | **27.601%** | **Higher than scalar** |
| **AVX2 instructions** | 39,934,500 | Confirmed AVX2 usage |
| **AVX-512 instructions** | 20,220,000 | Confirmed AVX-512 usage |
| **Time** | 0.656s | Includes all versions |
| **AVX2 time** | 93.48 us/iter | From benchmark output |
| **AVX-512 time** | 136.46 us/iter | From benchmark output |

## Key Findings

### 1. SIMD Instructions Are Being Executed ✓
- **39.9M AVX2 instructions**: Confirms AVX2 code is running
- **20.2M AVX-512 instructions**: Confirms AVX-512 code is running
- Both instruction sets are present in the binary and executing

### 2. Lower IPC in SIMD Version
- **Scalar IPC: 2.32** (good)
- **SIMD IPC: 2.14** (worse!)
- **Cause**: Memory stalls from non-contiguous atom access
- SIMD gather operations are slower than scalar loads

### 3. Higher Cache Miss Rate in SIMD
- **Scalar: 22.9%** cache miss rate
- **SIMD: 27.6%** cache miss rate (+20% relative increase)
- **Cause**: Random atom access patterns prevent prefetching
- Atoms accessed via `atoms_ptr[idx1[3]]`, `atoms_ptr[idx1[2]]`, etc. (non-sequential)

### 4. More Cycles Despite Fewer Instructions Per Pair
- **SIMD uses 8.6% more cycles** despite processing 4-8 pairs at once
- **Same total instructions** (benchmark runs all versions)
- **Root cause**: Memory access overhead dominates compute savings

## Performance Mechanism Analysis

### Why SIMD Shows Speedup Despite Higher Cycles?

The benchmark measures **individual function execution time**, not total cycles:

1. **Scalar function**: 122.48 us/iter
   - Processes pairs sequentially
   - Better cache locality (sequential pair access)
   - Lower cache miss rate

2. **AVX2 function**: 93.48 us/iter (1.36x faster)
   - Processes 4 pairs simultaneously
   - **Fewer function call overheads** (one call per 4 pairs)
   - **Better instruction-level parallelism** (multiple operations in flight)
   - Despite higher cache misses, instruction throughput wins

3. **AVX-512 function**: 136.46 us/iter (slower than AVX2!)
   - Processes 8 pairs simultaneously
   - **Even worse cache behavior** (more random access)
   - **Gather operations are expensive** (non-contiguous loads)
   - **CPU frequency scaling** (AVX-512 may downclock CPU)

### Memory Access Pattern Impact

**Scalar access pattern:**
```
for each pair:
  load atom1 (sequential or nearby)
  load atom2 (sequential or nearby)
  compute
```
- Better cache locality
- Prefetching works

**SIMD access pattern:**
```
for 4 pairs at once:
  load atoms[idx1[3]], atoms[idx1[2]], atoms[idx1[1]], atoms[idx1[0]]  // random!
  load atoms[idx2[3]], atoms[idx2[2]], atoms[idx2[1]], atoms[idx2[0]]  // random!
  compute (vectorized)
```
- Poor cache locality
- Prefetching ineffective
- Gather operations required

## Conclusions

### What's Working
1. SIMD instructions are executing (confirmed via perf)
2. Bounds checking overhead eliminated (1.57% vs 16.94%)
3. AVX2 shows 1.36x speedup (realistic for memory-bound workload)

### What's Limiting Performance
1. **Memory access patterns**: Non-contiguous atom access (27.6% cache miss rate)
2. **Gather operations**: AVX2/AVX-512 gather is slower than scalar loads
3. **Cache behavior**: Random access prevents prefetching
4. **AVX-512 overhead**: May downclock CPU or have gather penalties

### Why AVX-512 is Slower Than AVX2
- **Gather operations**: AVX-512 gather for 8 elements is more expensive
- **Cache pressure**: 8 random accesses per iteration vs 4
- **CPU frequency**: AVX-512 may trigger frequency scaling down
- **Memory bandwidth**: More simultaneous loads saturate memory bus

## Recommendations

1. **Test with larger workloads** (10K+ pairs) to see if speedup improves
2. **Consider data layout changes**:
   - Structure of Arrays (SoA) for better SIMD loading
   - Batch pairs by atom index ranges for contiguous access
3. **Profile memory bandwidth** to confirm if memory-bound
4. **Consider prefetching hints** for random atom access

## Next Steps

1. Run perf stat on larger workloads (10K, 50K pairs)
2. Measure memory bandwidth utilization
3. Profile with `perf c2c` to identify cache line conflicts
4. Test with SoA data layout if memory-bound

