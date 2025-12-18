# ConfEcalc Benchmark Steps

## Step 1: Profile Java Test That Calls C++ Energy Calculator

**Note:** The C++ unit tests (`confecalc_tests`) only test basic functionality and don't call energy calculations. We need to profile the Java test that actually exercises the energy calculation code.

```bash
# From repo root
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork

# Run Java test with JFR profiling
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  -PjvmArgs="-XX:+UnlockDiagnosticVMOptions -XX:+FlightRecorder -XX:StartFlightRecording=duration=60s,filename=energy-baseline.jfr"

# Analyze JFR
jfr summary energy-baseline.jfr | head -50
```

**Alternative: Profile C++ via Java with async-profiler:**
```bash
# Start test in background
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" &
PID=$!
sleep 5
JAVA_PID=$(pgrep -f "TestNativeConfEnergyCalculator")

# Profile (captures both Java and C++ code)
java -jar ~/async-profiler-2.9-linux-x64/async-profiler.jar -e cpu -d 30 -f energy-baseline.html $JAVA_PID

wait $PID
# Open energy-baseline.html in browser to see flame graph
```

**What to look for:**
- `calc_amber_eef1_f64` function calls
- `ambereef1::calc` or `calc_energy` functions
- Distance calculation functions
- Time spent in C++ vs Java

## Step 2: Run Java Test as Baseline

```bash
# From repo root
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork

# Run test with timing
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"
```

**Baseline Results (Recorded):**
- Test: `calcEnergy_native_all_2RL0_f64`
- Real time: 7m13.910s
- User time: 0m20.504s
- Sys time: 0m5.549s
- Status: PASSED
- Date: Baseline measurement

**What this measures:**
- 15 conformations with 7 positions each
- Calls `calc_amber_eef1_f64` for each conformation
- Exercises energy calculation loops in C++
- Includes JNA overhead and Java test framework overhead

**Next:** After implementing SIMD, run the same test and compare execution time.

## Step 3: Build Benchmark Executable

```bash
cd src/main/cc/ConfEcalc
cmake -B build -DBUILD_TESTS=ON
cmake --build build

# Verify benchmark was built
ls -la build/tests/benchmark_energy
```

## Step 4: (After SIMD Implementation) Compare Performance

```bash
# Run Java test with SIMD version
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"

# Compare with Step 2 baseline
```

---

## Quick Start (Run These Now)

**1. Baseline Java test timing:**
```bash
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork
time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"
```

**2. Profile Java test (captures C++ code execution):**
```bash
# Start test in background
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" &
PID=$!
sleep 5
JAVA_PID=$(pgrep -f "TestNativeConfEnergyCalculator")

# Profile with async-profiler (if available)
# java -jar ~/async-profiler-2.9-linux-x64/async-profiler.jar -e cpu -d 30 -f energy-baseline.html $JAVA_PID

# Or use JFR
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  -PjvmArgs="-XX:+UnlockDiagnosticVMOptions -XX:+FlightRecorder -XX:StartFlightRecording=duration=60s,filename=energy-baseline.jfr"

wait $PID
```

**Note:** The C++ unit tests don't exercise energy calculations. We must profile via the Java test that calls the native code.

