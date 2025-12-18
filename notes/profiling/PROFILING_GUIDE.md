# SIMD Performance Profiling Guide

## Overview
This guide explains how to profile and characterize the SIMD energy calculation performance to identify bottlenecks and optimize further.

## Available Benchmarks

### 1. Direct C++ Benchmark (`benchmark_simd_direct`)
Quick correctness and basic performance test:
```bash
cd src/main/cc/ConfEcalc
./build/benchmark_simd_direct 1000
```

### 2. Parameter Sweep Benchmark (`benchmark_simd_parameter_sweep`)
Sweeps different parameter ranges to characterize performance:
```bash
cd src/main/cc/ConfEcalc
./build/benchmark_simd_parameter_sweep 500 > results.csv
```

### 3. Java Integration Test
Full stack test (Java → JNA → C++):
```bash
export OSPREY_FORCE_SCALAR=1
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"
```

## Profiling Tools

### Linux `perf` Tool

#### Basic statistics:
```bash
perf stat ./build/benchmark_simd_direct 1000
```

#### Record performance data:
```bash
perf record -g ./build/benchmark_simd_direct 5000
perf report
```

#### Memory bandwidth analysis:
```bash
perf stat -e cache-misses,cache-references,LLC-loads,LLC-load-misses ./build/benchmark_simd_direct 5000
```

#### Check for SIMD instruction execution:
```bash
perf stat -e fp_arith_inst_retired.512b_packed_double,fp_arith_inst_retired.256b_packed_double ./build/benchmark_simd_direct 5000
```

### Intel VTune (if available)
More detailed analysis including roofline plots:
```bash
vtune -collect hotspots -result-dir vtune_result ./build/benchmark_simd_direct 5000
vtune -report summary -result-dir vtune_result
```

### Roofline Analysis

To create a roofline plot, we need:
1. **Peak compute performance** (FLOPs/sec)
   - AVX-512: ~1 TFLOP/s (8 doubles × 2 FMA units × ~60 GHz)
   - AVX2: ~500 GFLOP/s (4 doubles × 2 FMA units × ~60 GHz)
   - Scalar: ~120 GFLOP/s (2 FMA units × ~60 GHz)

2. **Peak memory bandwidth** (Bytes/sec)
   - DDR4: ~50 GB/s
   - DDR5: ~80 GB/s

3. **Compute intensity** (FLOPs/Byte)
   - Measure: FLOPs per memory operation
   - Plot: Performance vs Compute Intensity

## Performance Characterization

### Compute-Bound vs Memory-Bound

**Compute-bound characteristics:**
- Speedup scales with SIMD width
- Higher compute intensity (FLOPs/Byte) helps SIMD
- Memory bandwidth not fully utilized

**Memory-bound characteristics:**
- Speedup limited despite SIMD
- Lower compute intensity (memory bottleneck)
- Memory bandwidth fully utilized
- Cache misses dominate

### Current Observations

1. **Correctness**: ✓ All SIMD versions match scalar exactly
2. **AVX-512 present**: ✓ Confirmed via objdump (zmm registers)
3. **Performance**: Variable - sometimes slower, sometimes faster

### Potential Issues

1. **Memory access pattern**: Non-contiguous access to atoms array
   - Solution: Reorganize data for better cache locality
   
2. **Overhead**: Extracting from SIMD and calling scalar calc()
   - Solution: Vectorize the entire energy calculation

3. **Memory bandwidth**: Limited by random atom access
   - Solution: Prefetch or reorganize data structures

## Next Steps

1. **Run parameter sweep** to identify best/worst case scenarios
2. **Profile with perf** to find hotspots
3. **Measure cache misses** to identify memory bottlenecks
4. **Create roofline plot** to visualize compute vs memory limits
5. **Optimize data layout** if memory-bound
6. **Full SIMD vectorization** if compute-bound (vectorize calc() function, not just distance)

## Example Analysis Workflow

```bash
# 1. Run parameter sweep
cd src/main/cc/ConfEcalc
./build/benchmark_simd_parameter_sweep 500 > sweep_results.csv

# 2. Profile a specific case
perf record -g --call-graph dwarf ./build/benchmark_simd_direct 10000
perf report -g graph

# 3. Check SIMD instruction usage
perf stat -e fp_arith_inst_retired.512b_packed_double ./build/benchmark_simd_direct 10000

# 4. Analyze cache behavior
perf stat -e cache-misses,LLC-load-misses ./build/benchmark_simd_direct 10000
```

## Interpretation

- **If AVX-512 shows ~2x speedup**: Compute-bound, SIMD working well
- **If AVX-512 shows <1.2x speedup**: Memory-bound, need data layout optimization
- **If cache misses are high**: Memory access pattern needs improvement
- **If SIMD instructions not executed**: Runtime dispatch not working or CPU doesn't support

