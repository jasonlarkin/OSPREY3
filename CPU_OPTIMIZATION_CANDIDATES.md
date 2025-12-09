# CPU Optimization Candidates for OSPREY

## Best Candidates for SIMD/Threading Optimizations

### 1. ConfEcalc Energy Calculations (C++ - EXISTING CODE)

**Status:** Already in C++, **NO SIMD**, **NO threading**

**Location:** `src/main/cc/ConfEcalc/`

**Key Files:**
- `energy_ambereef1.h` - Force field energy calculations
- `confecalc.cc` - Main implementation with loops
- `motions/transrot.h` - Coordinate transformations with nested loops

**What It Does:**
- Pairwise energy calculations (electrostatic, van der Waals, EEF1 solvation)
- Distance calculations between atoms (x, y, z coordinates)
- Called millions of times during energy evaluation

**Optimization Opportunities:**

#### SIMD (High Impact)
- **Pairwise distance calculations:** Vectorize (x, y, z) coordinate operations
- **Energy accumulation:** Process multiple atom pairs simultaneously
- **Implementation:** AVX2 (4 doubles) or AVX-512 (8 doubles)
- **Expected speedup:** 2-4x for distance calculations, 1.5-3x overall

**Example loop to vectorize:**
```cpp
// Current (scalar):
for (int i = 0; i < n_atoms; i++) {
    for (int j = i+1; j < n_atoms; j++) {
        double dx = coords[i].x - coords[j].x;
        double dy = coords[i].y - coords[j].y;
        double dz = coords[i].z - coords[j].z;
        double dist = sqrt(dx*dx + dy*dy + dz*dz);
        energy += calculate_pairwise_energy(dist);
    }
}

// SIMD version (process 4 pairs at once):
// Use AVX2 to compute 4 distances simultaneously
```

#### Threading (Medium Impact)
- **Parallelize outer loop:** Process different atom pairs in parallel
- **Implementation:** OpenMP `#pragma omp parallel for`
- **Expected speedup:** 2-8x depending on CPU cores
- **Challenge:** Thread-safe energy accumulation (reduction)

**Why This Is Perfect:**
- Already in C++ (no porting needed)
- Hot loops identified (profiling can confirm)
- Clear computational pattern (pairwise operations)
- Easy to benchmark before/after
- Can demonstrate both SIMD and threading

**Profiling Strategy:**
1. Profile current implementation with Valgrind callgrind
2. Identify hottest loops in energy calculations
3. Add SIMD version
4. Benchmark and compare
5. Add OpenMP threading
6. Benchmark again

---

### 2. CCD Minimization (C++ - EXISTING CODE)

**Status:** Already in C++, **NO threading**

**Location:** `src/main/cc/ConfEcalc/minimization.h`

**What It Does:**
- Cyclic Coordinate Descent (CCD) iterative optimization
- Optimizes degrees of freedom (DOF) to minimize energy
- Iterates over DOFs until convergence

**Optimization Opportunities:**

#### Threading (High Impact)
- **Parallelize DOF evaluations:** Each DOF can be evaluated independently
- **Implementation:** OpenMP `#pragma omp parallel for`
- **Expected speedup:** 2-8x depending on number of DOFs and CPU cores
- **Effort:** Low (add OpenMP pragmas to existing loops)

**Example:**
```cpp
// Current (sequential):
for (int d = 0; d < dofs.get_size(); d++) {
    double energy = evaluate_dof(d);
    if (energy < best_energy) {
        best_energy = energy;
        best_dof = d;
    }
}

// OpenMP version:
#pragma omp parallel for reduction(min:best_energy)
for (int d = 0; d < dofs.get_size(); d++) {
    double energy = evaluate_dof(d);
    if (energy < best_energy) {
        best_energy = energy;
        best_dof = d;
    }
}
```

**Why This Is Perfect:**
- Already in C++
- Clear parallelization opportunity (independent DOF evaluations)
- Low effort (just add OpenMP pragmas)
- Easy to benchmark

---

### 3. EPIC Matrix Computation (Java - PORT TO C++)

**Status:** Currently Java, **NO SIMD**, partial threading

**Location:** `src/main/java/edu/duke/cs/osprey/ematrix/epic/`

**What It Does:**
- Polynomial evaluation for energy matrix precomputation
- Called by SAPE.java:93 (which also calls deepCopy)
- Computes energy values for conformation pairs

**Optimization Opportunities:**

#### Port to C++ with SIMD (High Impact)
- **Polynomial evaluation:** Vectorize coefficient operations
- **Matrix operations:** SIMD for array operations
- **Implementation:** Port Java code to C++, add AVX2/AVX-512
- **Expected speedup:** 2-4x from SIMD, plus C++ performance benefits

**Why This Is Good:**
- High computational cost (good target for optimization)
- Clear mathematical operations (polynomials)
- Can demonstrate C++ porting + SIMD optimization
- Requires porting from Java (more effort)

**Alternative:** Profile Java version first, then port hot functions to C++

---

## Comparison: Which to Choose?

### For Quick Demonstration (Existing C++ Code)

**Best Choice: ConfEcalc Energy Calculations**
- Already in C++
- No porting needed
- Can demonstrate both SIMD and threading
- Easy to benchmark
- High impact (called millions of times)

**Second Choice: CCD Minimization**
- Already in C++
- Very easy to add OpenMP (low effort)
- Clear parallelization opportunity
- Lower impact than energy calculations

### For Comprehensive Demonstration (Porting + Optimization)

**Best Choice: EPIC Matrix**
- High computational cost
- Demonstrates porting Java → C++
- Then add SIMD optimization
- Shows full optimization pipeline
- More effort (porting required)

---

## Recommended Approach

### Phase 1: Quick Win (ConfEcalc SIMD)
1. Profile ConfEcalc energy calculations
2. Identify hottest loops (pairwise distance calculations)
3. Add SIMD version using AVX2 intrinsics
4. Benchmark: Compare scalar vs SIMD
5. **Demonstrate:** 2-4x speedup with SIMD

### Phase 2: Threading (CCD Minimization)
1. Profile CCD minimization
2. Add OpenMP to DOF evaluation loop
3. Benchmark: Compare single-threaded vs multi-threaded
4. **Demonstrate:** 2-8x speedup with threading

### Phase 3: Combined (ConfEcalc SIMD + Threading)
1. Add OpenMP to SIMD-optimized energy calculations
2. Benchmark: Compare all versions
3. **Demonstrate:** Combined SIMD + threading speedup

### Phase 4: Porting + Optimization (EPIC Matrix)
1. Profile Java EPIC matrix computation
2. Port hot functions to C++
3. Add SIMD optimization
4. Benchmark: Compare Java vs C++ vs C+++SIMD
5. **Demonstrate:** Full optimization pipeline

---

## Profiling Commands

### Profile ConfEcalc (Current Implementation)
```bash
cd src/main/cc/ConfEcalc
cmake -B build -DBUILD_TESTS=ON
cmake --build build
cd build/tests

# Profile with Valgrind callgrind
valgrind --tool=callgrind --callgrind-out-file=confecalc-baseline.callgrind ./confecalc_tests

# Analyze - look for energy calculation functions
callgrind_annotate confecalc-baseline.callgrind | grep -E "(energy|distance|pairwise|calc_)" > energy-functions.txt
```

### Profile Java Energy Calculator (Calls C++)
```bash
# This exercises ConfEcalc from Java
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator" &
PID=$!
sleep 5
JAVA_PID=$(pgrep -f "TestNativeConfEnergyCalculator")

# Profile Java side (shows JNA overhead, C++ call patterns)
java -jar ~/async-profiler-2.9-linux-x64/async-profiler.jar -e cpu -d 30 -f energy-calc-cpu.html $JAVA_PID
wait $PID
```

---

## Expected Results

### ConfEcalc SIMD Optimization
- **Baseline:** Scalar pairwise energy calculations
- **Optimized:** SIMD (AVX2) processes 4 atom pairs simultaneously
- **Expected speedup:** 2-4x for distance calculations
- **Measurable:** Yes, with clear before/after benchmarks

### CCD Minimization Threading
- **Baseline:** Sequential DOF evaluation
- **Optimized:** OpenMP parallel DOF evaluation
- **Expected speedup:** 2-8x (depends on CPU cores and DOF count)
- **Measurable:** Yes, with clear before/after benchmarks

### EPIC Matrix (Port + SIMD)
- **Baseline:** Java polynomial evaluation
- **Port to C++:** 1.5-2x speedup (C++ vs Java)
- **Add SIMD:** Additional 2-4x speedup
- **Total:** 3-8x speedup over Java version
- **Measurable:** Yes, with clear before/after benchmarks

---

## Implementation Notes

### SIMD Requirements
- **Compiler:** GCC 4.9+ or Clang 3.7+ (AVX2 support)
- **CPU:** Intel Haswell+ or AMD Excavator+ (AVX2)
- **Flags:** `-mavx2 -mfma` for AVX2, `-mavx512f` for AVX-512
- **Intrinsics:** `<immintrin.h>` for AVX2, `<x86intrin.h>` for general

### OpenMP Requirements
- **Compiler:** GCC 4.9+ or Clang 3.7+ (OpenMP support)
- **Flags:** `-fopenmp` for GCC, `-fopenmp=libomp` for Clang
- **Runtime:** `libgomp` (GCC) or `libomp` (Clang)

### CMake Configuration
```cmake
# Enable SIMD
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")

# Enable OpenMP
find_package(OpenMP REQUIRED)
target_link_libraries(ConfEcalc PRIVATE OpenMP::OpenMP_CXX)
```

---

## Summary

**Best Candidate for Quick Demonstration:**
1. **ConfEcalc Energy Calculations** - Already C++, add SIMD, easy to benchmark

**Best Candidate for Comprehensive Demo:**
2. **EPIC Matrix** - Port Java → C++ → Add SIMD, shows full pipeline

**Easiest Win:**
3. **CCD Minimization** - Add OpenMP, very low effort, clear benefit

**Recommended Order:**
1. Start with ConfEcalc SIMD (quick win, existing code)
2. Add CCD OpenMP (easy threading demo)
3. Then consider EPIC Matrix port (comprehensive demo)

