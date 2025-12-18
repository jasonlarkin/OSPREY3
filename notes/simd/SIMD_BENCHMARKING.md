# SIMD Benchmarking Guide

## Tests That Exercise Energy Calculations

### Main Test Suite
**File:** `src/test/java/edu/duke/cs/osprey/energy/compiled/TestNativeConfEnergyCalculator.java`

1. **`calcEnergy_native_all_2RL0_f64`** (Line 221)
   - 15 conformations
   - 7 positions per conformation
   - Calls `calc_amber_eef1_f64` in C++
   - **Best for benchmarking** - Pure energy calculation, no minimization overhead

2. **`calcEnergy_native_all_1DG9_6f_f64`** (Line 223)
   - 7 conformations
   - 5 positions per conformation
   - Calls `calc_amber_eef1_f64` in C++
   - Alternative test case

3. **`minimizeEnergy_native_2RL0_f64`** (Line 438)
   - 15 conformations with CCD minimization
   - Calls `minimize_amber_eef1_f64` in C++
   - Includes minimization overhead (not pure energy calculation)

### Benchmark Suite
**File:** `src/test/java/edu/duke/cs/osprey/BenchmarkEnergies.java`
- General benchmarking infrastructure
- **Currently does NOT compare scalar vs SIMD directly**

## Current Status: No Scalar/SIMD Comparison

**Problem:** The runtime dispatch automatically selects the best available SIMD version (AVX-512 → AVX2 → Scalar) based on CPU capabilities. There's **no way to force scalar execution** for baseline comparison.

## Force Scalar Mode (Implemented)

**Environment Variable:** `OSPREY_FORCE_SCALAR=1`

Runtime flag to force scalar execution for benchmarking. When set, the runtime dispatch will use the scalar implementation regardless of CPU capabilities.

**Usage:**
```bash
# Force scalar (baseline)
OSPREY_FORCE_SCALAR=1 ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"

# Use SIMD (default, auto-detect best available)
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"

# Export for multiple commands
export OSPREY_FORCE_SCALAR=1
./gradlew test --tests "..."
unset OSPREY_FORCE_SCALAR
```

**Implementation:** The check happens at first call in `calc_dispatch()`. Once initialized, the function pointer is cached (lazy initialization), so set the environment variable before running tests.

### Option 2: Build-Time Flag

Add CMake option to disable SIMD entirely:

```cmake
option(FORCE_SCALAR_ONLY "Force scalar-only build (disable SIMD)" OFF)
if(FORCE_SCALAR_ONLY)
    set(ENABLE_SIMD OFF)
endif()
```

**Usage:**
```bash
# Build scalar-only version
cmake -B build -DFORCE_SCALAR_ONLY=ON

# Build with SIMD (default)
cmake -B build -DENABLE_SIMD=ON
```

**Limitation:** Requires separate builds, can't compare in single binary.

### Option 3: Java System Property

Add JNA callable function to set scalar mode:

```java
// In Java test
System.setProperty("osprey.forceScalar", "true");
```

## Recommended: Create Benchmark Test

Create a dedicated benchmark test that compares all versions:

```java
public class BenchmarkSimdVersions {
    @Test
    public void compareScalarVsSimd() {
        // Test with scalar forced
        System.setenv("OSPREY_FORCE_SCALAR", "1");
        long scalarTime = benchmarkEnergyCalculation();
        
        // Test with SIMD (auto-detect)
        System.clearProperty("osprey.forceScalar");
        long simdTime = benchmarkEnergyCalculation();
        
        double speedup = (double)scalarTime / simdTime;
        System.out.printf("SIMD speedup: %.2fx (scalar: %dms, SIMD: %dms)\n", 
                         speedup, scalarTime, simdTime);
    }
}
```

## Implementation Priority

1. **High:** Add environment variable `OSPREY_FORCE_SCALAR` to force scalar mode
2. **Medium:** Create `BenchmarkSimdVersions` test class
3. **Low:** Document benchmarking procedure

## Current Benchmarking Approach (Workaround)

Since we can't force scalar mode yet, current approach is:

1. **Build without SIMD** (scalar baseline):
   ```bash
   cd src/main/cc/ConfEcalc
   cmake -B build -DENABLE_SIMD=OFF
   cmake --build build
   ./gradlew test --tests "TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"
   ```

2. **Build with SIMD** (optimized):
   ```bash
   cd src/main/cc/ConfEcalc
   cmake -B build -DENABLE_SIMD=ON
   cmake --build build
   ./gradlew test --tests "TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"
   ```

**Limitation:** Requires two separate builds, can't compare in same process.

