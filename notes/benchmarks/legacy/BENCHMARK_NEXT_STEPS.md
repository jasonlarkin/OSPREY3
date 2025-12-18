# Benchmark Next Steps

## Current Status

### ConfEcalc SIMD (AVX2)
- **Status:** Implemented and tested
- **Baseline:** 19.885s
- **SIMD:** 19.820s (1.003x speedup)
- **Result:** Minimal improvement, likely due to memory bandwidth bottleneck

## Benchmark Options

### Option 1: Multiple Runs for Statistics
Run the same test multiple times to get average and variance:

```bash
# Run 10 times and collect timing
for i in {1..10}; do
  time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" 2>&1 | grep "real" | awk '{print $2}'
done
```

**Purpose:** Establish statistical confidence in measurements

### Option 2: Profile to Understand Bottleneck
Profile the C++ code to see where time is actually spent:

```bash
cd src/main/cc/ConfEcalc/build/tests
valgrind --tool=callgrind --callgrind-out-file=simd-profile.callgrind ./confecalc_tests
callgrind_annotate simd-profile.callgrind | head -50
```

**Purpose:** Identify if SIMD is actually being used, or if memory access is the bottleneck

### Option 3: Test Other Test Cases
Benchmark other test methods that might show better speedup:

```bash
# Test with different conformation set
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64"

# Test minimization (includes CCD loops)
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64"
```

**Purpose:** Find test cases where SIMD shows better improvement

### Option 4: Try AVX-512 Instead of AVX2
CPU supports AVX-512 (8 doubles vs 4 with AVX2):

```bash
cd src/main/cc/ConfEcalc
# Modify CMakeLists.txt to use AVX-512
cmake -B build -DENABLE_SIMD=ON -DCMAKE_CXX_FLAGS="-mavx512f -mavx512dq"
cmake --build build
```

**Purpose:** Potential 2x improvement over AVX2 (8 doubles at once)

### Option 5: Benchmark CCD Minimization (Next Candidate)
Move to threading optimization for CCD minimization:

```bash
# Baseline
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64"

# Then add OpenMP threading to CCD loops
```

**Purpose:** Demonstrate threading optimization (expected 2-8x speedup)

### Option 6: C++ Direct Benchmark (Bypass JNA)
Create standalone C++ benchmark to eliminate JNA overhead:

```bash
cd src/main/cc/ConfEcalc/build/tests
./benchmark_energy  # If implemented
```

**Purpose:** Measure pure C++ performance without Java/JNA overhead

## Recommended Order

1. **Profile SIMD implementation** (Option 2)
   - Verify SIMD code is actually executing
   - Identify memory access patterns
   - Understand why speedup is minimal

2. **Multiple runs for statistics** (Option 1)
   - Establish baseline variance
   - Confirm measurements are consistent

3. **Try AVX-512** (Option 4)
   - Potential 2x improvement over AVX2
   - CPU supports it

4. **Move to CCD threading** (Option 5)
   - Different optimization approach
   - Expected better speedup (2-8x)

## Quick Commands

### Profile Current SIMD Implementation
```bash
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork/src/main/cc/ConfEcalc/build/tests
valgrind --tool=callgrind --callgrind-out-file=simd.callgrind ./confecalc_tests
callgrind_annotate simd.callgrind | grep -E "(calc_simd|calc_|energy)" | head -20
```

### Run Multiple Benchmark Iterations
```bash
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork
for i in {1..5}; do
  echo "Run $i:"
  time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" 2>&1 | grep "real"
done
```

### Test Minimization (CCD)
```bash
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64"
```

