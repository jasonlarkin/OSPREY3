# Comprehensive Benchmarking Plan

## Current Issues
1. Too few iterations (1000) - not enough for statistical significance
2. Only one system size (200 atoms, 2000 amber pairs, 1000 eef1 pairs)
3. Not based on real OSPREY workloads
4. Separate benchmarks needed for accurate profiling

## System Sizes from OSPREY Tests

Based on TEST_INVENTORY.md and test analysis:

### 2RL0 Example (KStar test)
- **Complex**: 16,734 pairs (from TestKStar.java)
- **Ligand**: 7,575 pairs
- **Protein**: 1,008 pairs

### 1CC8 Example (GMEC test)
- Used extensively in tests
- Need to determine actual atom/pair counts

### TestNativeConfEnergyCalculator
- Uses 2RL0 and 1DG9_6f test cases
- These are the primary benchmarks for energy calculation

## Recommended Benchmark Configuration

### System Sizes (atoms, amber_pairs, eef1_pairs)

**Small (current)**
- Atoms: 200
- Amber pairs: 2,000
- EEF1 pairs: 1,000
- Total pairs: 3,000

**Medium (2RL0 protein-like)**
- Atoms: 500
- Amber pairs: 1,000
- EEF1 pairs: 500
- Total pairs: 1,500

**Large (2RL0 ligand-like)**
- Atoms: 1,000
- Amber pairs: 7,500
- EEF1 pairs: 500
- Total pairs: 8,000

**XLarge (2RL0 complex-like)**
- Atoms: 2,000
- Amber pairs: 15,000
- EEF1 pairs: 2,000
- Total pairs: 17,000

**XXLarge (stress test)**
- Atoms: 5,000
- Amber pairs: 50,000
- EEF1 pairs: 5,000
- Total pairs: 55,000

### Iterations per Test
- **Warmup**: 100 iterations (per version)
- **Benchmark**: 10,000 iterations minimum
  - Small systems: 50,000 iterations
  - Medium systems: 20,000 iterations
  - Large systems: 10,000 iterations
  - XLarge systems: 5,000 iterations
  - XXLarge systems: 1,000 iterations

### Statistical Analysis
- Run each configuration **5-10 times** (different random seeds)
- Calculate: mean, median, min, max, stddev, confidence intervals
- Report speedup with error bars

## Implementation

### Benchmark Structure
1. **Separate executables** for scalar, AVX2, AVX-512
2. **Parameter sweep** across system sizes
3. **Multiple runs** with different seeds for statistics
4. **Output CSV** for analysis and plotting

### CSV Output Format
```
version,system_size,atoms,amber_pairs,eef1_pairs,iterations,time_us_per_iter,time_total_ms,run_id
scalar,small,200,2000,1000,50000,122.48,6124.0,0
avx2,small,200,2000,1000,50000,93.48,4674.0,0
avx512,small,200,2000,1000,50000,136.46,6823.0,0
...
```

### Performance Metrics
- Time per iteration (us)
- Total time (ms)
- Speedup vs scalar
- IPC (instructions per cycle) - from perf stat
- Cache miss rate - from perf stat
- Memory bandwidth - from perf stat

## Next Steps

1. Fix compilation errors (namespace pollution)
2. Create comprehensive benchmark script
3. Run benchmarks across all system sizes
4. Analyze results with statistics
5. Generate performance plots

