# Google Colab Benchmarking Guide

## Why Use Colab for Benchmarking?

Running benchmarks in Google Colab provides cleaner CPU/memory isolation compared to WSL on Windows:

### Issues with Local WSL/Windows Environment
- **Windows background processes** constantly consuming resources (Antimalware, Windows Update, etc.)
- **WSL virtualization overhead** - additional layer between hardware and benchmark
- **Resource contention** - Cursor, Chrome, and other apps competing for CPU/memory
- **CPU frequency scaling** affected by system load
- **Non-deterministic cache behavior** due to shared resources

### Benefits of Colab
- **Clean Linux environment** - minimal background processes
- **Better CPU isolation** - dedicated CPU cores per session
- **Consistent hardware** - same CPU architecture across runs
- **Reproducible results** - less variance from system interference
- **No virtualization overhead** - direct Linux kernel access

## Setup Instructions

### 1. Upload OSPREY Code to Colab

```python
# In a Colab notebook cell:
!git clone https://github.com/your-repo/osprey-fork_fresh.git
# Or upload the directory manually
```

### 2. Install Dependencies

```python
# Run setup script
!cd osprey-fork_fresh && python3 scripts/tools/colab_benchmark_setup.py
```

### 3. Build Benchmarks

```python
# Build with SIMD support
!cd osprey-fork_fresh/src/main/cc/ConfEcalc && \
  cmake -B build -DENABLE_SIMD=ON && \
  cmake --build build
```

### 4. Run Benchmarks

```python
# Run comprehensive benchmarks
!cd osprey-fork_fresh && \
  ./scripts/tools/benchmark_comprehensive.sh 5
```

### 5. Generate Plots

```python
# Generate all performance plots
!cd osprey-fork_fresh && \
  python3 scripts/tools/plot_benchmark_results.py && \
  python3 scripts/tools/plot_roofline_combined.py && \
  python3 scripts/tools/plot_cache_hierarchy.py medium
```

### 6. Download Results

```python
# Download results CSV files
from google.colab import files

files.download('osprey-fork_fresh/benchmark_results.csv')
files.download('osprey-fork_fresh/arithmetic_intensity_measurements.csv')

# Download plots
import os
for plot in os.listdir('osprey-fork_fresh/plots'):
    files.download(f'osprey-fork_fresh/plots/{plot}')
```

## Quick Benchmark Script for Colab

Create a Colab notebook with this:

```python
import subprocess
import sys

# 1. Setup
print("Setting up environment...")
subprocess.run([sys.executable, "-m", "pip", "install", "-q", "matplotlib", "numpy"])

# 2. Build
print("\nBuilding benchmarks...")
subprocess.run(["bash", "-c", "cd osprey-fork_fresh/src/main/cc/ConfEcalc && cmake -B build -DENABLE_SIMD=ON && cmake --build build"], check=True)

# 3. Run medium system benchmark (focus on AVX-512 anomaly)
print("\nRunning medium system benchmark...")
subprocess.run(["bash", "-c", "cd osprey-fork_fresh && ./scripts/tools/benchmark_medium_system.sh 10"], check=True)

# 4. Measure arithmetic intensity
print("\nMeasuring arithmetic intensity...")
subprocess.run(["bash", "-c", "cd osprey-fork_fresh && ./scripts/tools/measure_arithmetic_intensity.sh medium"], check=True)

# 5. Analyze cache hierarchy
print("\nAnalyzing cache hierarchy...")
subprocess.run(["bash", "-c", "cd osprey-fork_fresh && ./scripts/tools/analyze_cache_hierarchy.sh medium"], check=True)

# 6. Generate plots
print("\nGenerating plots...")
subprocess.run(["bash", "-c", "cd osprey-fork_fresh && python3 scripts/tools/plot_benchmark_results.py"], check=True)
subprocess.run(["bash", "-c", "cd osprey-fork_fresh && python3 scripts/tools/plot_cache_hierarchy.py medium"], check=True)

print("\n=== Benchmarking Complete ===")
print("Check osprey-fork_fresh/benchmark_results.csv for results")
```

## Expected Improvements in Colab

### Lower Variance
- **WSL/Windows**: 41.8% CV for AVX-512 on medium (high variance)
- **Colab**: Expected <10% CV (cleaner environment)

### More Consistent Speedups
- **WSL/Windows**: AVX-512 shows 0.70x speedup on medium (slowdown)
- **Colab**: Should show consistent speedup pattern (may still be slower than AVX2, but more predictable)

### Better Cache Behavior
- Less interference from background processes
- More deterministic cache hit/miss rates
- Clearer picture of actual SIMD performance

## Comparing Results

After running in Colab, compare with local results:

```python
import pandas as pd
import matplotlib.pyplot as plt

# Load local and Colab results
local_results = pd.read_csv('local_benchmark_results.csv')
colab_results = pd.read_csv('colab_benchmark_results.csv')

# Compare speedups
comparison = pd.DataFrame({
    'local_avx2': local_results[local_results['version']=='avx2']['speedup'],
    'colab_avx2': colab_results[colab_results['version']=='avx2']['speedup'],
    'local_avx512': local_results[local_results['version']=='avx512']['speedup'],
    'colab_avx512': colab_results[colab_results['version']=='avx512']['speedup'],
})

print("Speedup Comparison:")
print(comparison.describe())
```

## Notes

- **Colab CPU**: Typically Intel Xeon (may not have AVX-512 on all instances)
- **Session limits**: Free tier has usage limits
- **File persistence**: Upload code at start of each session
- **Performance**: Colab CPUs are usually optimized for performance mode

## Next Steps

1. Run benchmarks in Colab to get baseline
2. Compare variance between WSL and Colab
3. Determine if AVX-512 slowdown on medium is hardware-specific or environmental
4. Use Colab results as "clean" reference for optimization decisions

