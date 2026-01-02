# C++ Native Code Profiling Results Summary

## Overview

This document summarizes the current state of ConfEcalc C++ profiling/benchmarking for SIMD (AVX2/AVX-512) energy calculations.

**Status update (2026-01-01):** SIMD is now **consistently faster than scalar** on the reference microbenchmark workload after fixing the SIMD hot-path overheads and stabilizing the benchmark/profiling harnesses.

## Key Components Profiled

### 1. ConfEcalc Energy Calculations

**Location:** `src/main/cc/ConfEcalc/energy_ambereef1.h`

**What It Does:**
- Pairwise energy calculations (electrostatic, van der Waals, EEF1 solvation)
- Distance calculations between atoms (x, y, z coordinates)
- Called millions of times during energy evaluation

**Status:**
- SIMD implementation exists (AVX2 and AVX-512)
- Current performance (microbench): **faster than scalar**
  - AVX2 is typically the best default on this CPU/workload
  - AVX-512 can be close but may be slightly slower than AVX2 (frequency/downclock + overhead)
- Runtime dispatch is implemented (opt-in) via environment variables:
  - `OSPREY_USE_SIMD=1` enables runtime SIMD dispatch (default is scalar for reproducibility)
  - `OSPREY_FORCE_SCALAR=1` forces scalar
  - `OSPREY_USE_FAST_EXP=1` enables fast-exp variants (not usable for production due to large error)
- “Memory-bound” should be interpreted carefully:
  - this kernel is largely limited by *memory-system effects and expensive scalar exp for EEF1*, not raw DRAM bandwidth.

### 2. CCD Minimization

**Location:** `src/main/cc/ConfEcalc/minimization.h`

**What It Does:**
- Cyclic Coordinate Descent iterative optimization
- Optimizes degrees of freedom (DOF) to minimize energy
- Sequential line search over DOFs

**Status:**
- OpenMP threading added (Phase 2 complete)
- Expected speedup: 2-4x on 4-core CPU
- Benchmarking pending

### 3. Coordinate Transformations

**Location:** `src/main/cc/ConfEcalc/motions/transrot.h`

**What It Does:**
- Translation and rotation of atom coordinates
- Matrix-vector multiplications
- Applied to multiple atoms in loops

**Status:**
- SIMD implementation in progress
- Files created: `real3_simd.h`, `rotation_simd.h`, `transrot_simd.h`
- Expected speedup: 2-4x (AVX2/AVX-512)

## Benchmark Results

### Energy Calculation Performance

**Test Configuration:** 200 atoms, 2000 Amber pairs, 1000 EEF1 pairs

**Deterministic reproducibility sweep (WSL, pinned to one core, 15 reps):**

| Version | Mean (us/iter) | Stdev (us/iter) | Speedup vs Scalar |
|---------|-----------------|------------------|-------------------|
| Scalar (baseline) | 30.44 | 0.98 | 1.00x |
| AVX2 (exact exp) | 21.21 | 1.19 | 1.44x |
| AVX-512 (exact exp) | 22.11 | 0.55 | 1.38x |

**Notes:**
- AVX2 is ~4% faster than AVX-512 on this workload on the i3-1115G4 under WSL.
- `benchmark_simd_direct` (same workload, 5000 iters) reports similar speedups and validates correctness.

### Correctness

- AVX2 (exact exp): Correct (relative error 1.99e-16)
- AVX-512 (exact exp): Correct (relative error 7.97e-16)
- Fast exp approximation: **Unusable** (14.1x error)

## Performance Bottlenecks Identified

### 1. Scalar exp() Pipeline Breaks (33% of pairs)

**Problem:** EEF1 pairs use scalar `std::exp()` calls, breaking SIMD pipeline

**Impact:**
- 10.36% of time in `AtomPairEef1::calc` (scalar calls)
- Forces pipeline flush every 4 pairs
- Defeats SIMD optimization for EEF1 section

**Evidence:** SIMD implementation still must call scalar `std::exp` per lane for exactness.

### 2. Array Bounds Checking Overhead (17% of time)

**Problem:** Every `atoms[idx]` access calls `Array::operator[]` with assertions

**Impact:**
- Previously large overhead in the SIMD hot path.

**Status:** Fixed in SIMD hot path by using `atoms.pointer()` + once-per-iteration bounds asserts.

### 3. Non-Contiguous Memory Access

**Problem:** Atoms are accessed in a non-contiguous pattern (indexed loads), limiting locality and SIMD scaling.

**Impact:**
- Cache/locality limits reduce AVX-512’s benefit relative to AVX2.

**Solution:** Structure of Arrays (SoA) layout or batch processing

### 4. Small Workload Size

**Problem:** 3000 pairs may not amortize SIMD setup overhead

**Impact:**
- Setup overhead: 40-65 cycles per 4/8 pairs
- Small workloads favor scalar (better compiler optimization)

**Status:** Not the primary limiter here (3000 pairs is enough to show SIMD speedup after fixes), but larger workloads are still required to study scaling and AVX-512 crossover behavior.

### 5. Memory Bandwidth

**Analysis:**
- Raw DRAM bandwidth is not saturated in these microbenchmarks.
- The practical limiter is *memory-system behavior + scalar exp overhead + ISA-specific frequency behavior*, not peak FLOPs.

## Profiling Tools Used

### Existing Tools (in `scripts/tools/`)

1. **`run_all_simd_analysis.sh`** - Complete SIMD analysis pipeline
   - Runs benchmarks
   - Measures arithmetic intensity
   - Generates roofline plots
   - Analyzes cache hierarchy

2. **`profile_simd.sh`** - Perf profiling of SIMD implementations
   - Profiles scalar, AVX2, AVX-512 versions
   - Generates perf reports and call graphs

3. **`analyze_perf_profiles.sh`** - Analysis and comparison
   - Extracts top functions
   - Compares versions
   - Checks SIMD instruction usage

4. **`benchmark_comprehensive.sh`** - Comprehensive benchmarking
   - Multiple system sizes
   - Statistical analysis

5. **`measure_arithmetic_intensity.sh`** - Memory bandwidth analysis
   - Calculates FLOPs/byte
   - Roofline plot generation

6. **`analyze_cache_hierarchy.sh`** - Cache performance analysis
   - L1, L2, L3 cache statistics
   - Cache miss analysis

7. **`repro_simd_wsl.sh`** - Deterministic, CPU-pinned reproducibility sweeps (WSL-friendly)
   - Repeats `benchmark_*_only` binaries with fixed dataset
   - Runs `perf stat -x,` and saves CSV output

8. **`plot_repro_simd_results.py`** - Summarize/plot reproducibility sweep outputs
   - Writes `repro_summary.csv` and basic plots into the sweep directory

### Results Location

- **Benchmark results:** `benchmark_results.csv` (repo root)
- **Arithmetic intensity:** `arithmetic_intensity_measurements.csv`
- **Perf profiles:** `src/main/cc/ConfEcalc/perf_results/`
- **Plots:** `plots/` directory (roofline, cache hierarchy)
- **Analysis docs:** `notes/simd/` directory

## Key Findings

### SIMD Performance (current)

1. **SIMD is faster than scalar on the reference microbenchmark**
   - AVX2: ~1.44x
   - AVX-512: ~1.38x
   - AVX-512 is slightly slower than AVX2 on this CPU/workload (common on client CPUs)

2. **AVX-512 scaling is limited**
   - Indexed atom loads + scalar exact exp for EEF1 reduce the advantage of wider vectors.

3. **Fast exp approximation unusable**
   - 14.1x error with Taylor series
   - Must use exact `std::exp()` for force field accuracy

### Optimization Opportunities

1. **High Priority:**
   - Vectorize exact exp for EEF1 (requires an accurate SIMD exp implementation/library)
   - Improve memory access patterns (pair reordering / cache blocking / SoA)
   - Re-run on native Linux/AWS for cleaner perf + better process isolation

2. **Medium Priority:**
   - Optimize reductions / remove remaining avoidable stores in hot loops
   - Expand the reproducibility sweep across multiple system sizes (small/medium/large/…)

3. **Low Priority:**
   - Optimize horizontal reduction (2-5% gain)
   - Reduce function call overhead

### Threading Status

- **CCD Minimization:** OpenMP added, benchmarking pending
- **Energy Calculations:** Not yet threaded
- **Expected speedup:** 2-4x on 4-core CPU for CCD

## Recommendations

### Immediate Actions

1. Use the reproducibility harness (`scripts/tools/repro_simd_wsl.sh`) to lock down baseline variance.
2. Run the same harness on AWS (native Linux) to avoid WSL virtualization effects and get stable perf counters.

### Medium-term

4. **Improve memory access patterns** (SoA layout or batching)
5. **Complete coordinate transformation SIMD** (in progress)
6. **Benchmark CCD threading** performance

### Long-term

7. **Vectorize EEF1 exp()** (requires accurate SIMD exp implementation)
8. **Cache blocking** for better memory locality
9. **Pair reordering** for improved spatial access

## Integration with Full K* Analysis

### CPU vs Memory Bottlenecks

**From Full K* GC Analysis:**
- GC overhead: 0.51-3.40% (memory is not a bottleneck)
- Focus should be on CPU-bound operations

**From C++ Profiling:**
- Energy calculations are CPU-intensive
- SIMD optimization opportunities exist
- Memory access patterns need improvement

**Conclusion:**
- **Memory:** Not a bottleneck (GC overhead low)
- **CPU:** Energy calculations are the bottleneck
- **Optimization focus:** C++ energy calculation SIMD and threading

## Next Steps

1. Run `repro_simd_wsl.sh` across multiple system sizes to locate any AVX-512 crossover regions.
2. Run the same suite on AWS (native Linux) and compare:
   - mean/stdev
   - AVX2 vs AVX-512 ordering
   - perf stat derived IPC/cache metrics
3. If AVX-512 remains worse than AVX2 on native Linux for most workloads, treat AVX2 as the default “best” SIMD target and consider AVX-512 opt-in.

## Documentation References

- `notes/simd/SIMD_PERFORMANCE_ANALYSIS.md` - Detailed performance analysis
- `notes/simd/SIMD_BENCHMARK_RESULTS.md` - Benchmark results
- `notes/simd/SIMD_PERFORMANCE_BOTTLENECKS.md` - Bottleneck analysis
- `notes/simd/SIMD_OPTIMIZATION_ROADMAP.md` - Implementation roadmap
- `docs/performance/CPU_OPTIMIZATION_CANDIDATES.md` - Optimization opportunities
- `docs/performance/CCD_MINIMIZATION_REVIEW.md` - CCD algorithm review

