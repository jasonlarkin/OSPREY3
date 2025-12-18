# Benchmark Profiling Results

## Step 1: Profile SIMD Implementation

### Attempt 1: Profile C++ Unit Tests
**Command:**
```bash
cd src/main/cc/ConfEcalc/build/tests
valgrind --tool=callgrind --callgrind-out-file=simd-profile.callgrind ./confecalc_tests
```

**Result:**
- Profile completed successfully
- 3,565,035 instruction references collected
- **Issue:** Unit tests don't call energy calculation functions
- Profile shows mostly:
  - Dynamic linker overhead (30.62%)
  - Google Test framework code
  - Memory allocation/deallocation

**Conclusion:** Unit tests are too lightweight - they only test basic functionality (Array operations, version functions).

### Next Steps for Profiling

#### Option A: Profile Java Test (Calls C++ via JNA)
**Challenge:** Valgrind doesn't work well with Java. Need Java-specific profiler.

**Tools:**
- JFR (Java Flight Recorder) - Built into JDK
- async-profiler - Low-overhead Java profiler
- VisualVM - Java profiler

**Command:**
```bash
# Run Java test with JFR
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  -PjvmArgs="-XX:+UnlockDiagnosticVMOptions -XX:+FlightRecorder -XX:StartFlightRecording=duration=60s,filename=energy-test.jfr"
```

#### Option B: Create C++ Benchmark
Create standalone C++ benchmark that directly calls energy calculation functions, bypassing JNA overhead.

**Location:** `src/main/cc/ConfEcalc/tests/benchmark_energy.cpp`
**Status:** Placeholder exists, needs implementation

#### Option C: Verify SIMD at Compile Time
Check if SIMD code is actually compiled in:

```bash
# Check if USE_SIMD is defined
grep -r "USE_SIMD" src/main/cc/ConfEcalc/build/

# Check assembly output for AVX2 instructions
objdump -d src/main/resources/linux-x86-64/libConfEcalc.so | grep -E "(vaddpd|vmulpd|vsubpd)" | head -20
```

### Verification: Is SIMD Actually Being Used?

**Check 1: Compile-time flags**
- CMake cache shows: `ENABLE_SIMD:BOOL=ON`
- Compiler flags should include: `-mavx2 -mfma`
- Preprocessor should define: `USE_SIMD`

**Check 2: Runtime verification**
- Add debug output in `calc_simd` function
- Or check assembly for AVX2 instructions
- Or use `perf` to count AVX2 instructions (if available)

**Check 3: Performance characteristics**
- If SIMD is working, should see:
  - 2-4x speedup for compute-bound sections
  - Higher CPU utilization
  - Different cache behavior

**Current observation:**
- Minimal speedup (1.003x) suggests:
  - SIMD may not be executing (falling back to scalar)
  - Memory bandwidth bottleneck
  - JNA overhead masking improvements
  - Small iteration counts (SIMD overhead dominates)

### Verification Results

**SIMD Compilation Verified:**
```bash
# Checked binary for AVX2 instructions
objdump -d src/main/resources/linux-x86-64/libConfEcalc.so | grep -E "(vaddpd|vmulpd|vsubpd|vbroadcastsd|vmovapd|vfmadd|vsqrtpd)" | wc -l
# Result: 128 AVX2 instructions found
```

**Conclusion:** SIMD code is definitely compiled into the binary. AVX2 instructions are present.

**Compile Flags:**
- CMake cache shows: `ENABLE_SIMD:BOOL=ON`
- CMAKE_CXX_FLAGS includes: `-pg -O2` (plus AVX2 flags from CMakeLists.txt)
- Binary contains 128 AVX2 instructions

### Recommendations

2. **Add instrumentation:**
   - Add debug prints in `calc_simd` to verify it's called
   - Or use `__builtin_cpu_supports("avx2")` to check at runtime

3. **Profile Java test:**
   - Use async-profiler to see C++ function calls from Java
   - Identify if `calc_simd` is actually being invoked

4. **Create C++ benchmark:**
   - Direct C++ benchmark bypasses JNA overhead
   - More accurate performance measurement
   - Easier to profile with Valgrind

