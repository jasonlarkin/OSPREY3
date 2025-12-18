# SIMD Optimization Roadmap

## Current C++ Components

### 1. ConfEcalc (CPU Energy Calculator)
**Location:** `src/main/cc/ConfEcalc/`

**Status:** SIMD Optimized (In Progress)
- **Energy Calculation (`calc_amber_eef1`):** Fully vectorized (AVX2/AVX-512)
  - Amber pair interactions: Vectorized
  - EEF1 pair interactions: Vectorized (with exact exp)
  - Fast exp approximations: Separate versions for testing
  - **Speedup achieved:** 1.35x (AVX-512) vs scalar
  - **Arithmetic Intensity:** 0.003-0.005 FLOPs/byte (memory-bound)

**Remaining Opportunities:**
- Cache blocking for better memory access patterns
- Pair reordering for improved spatial locality
- Explicit prefetching hints

### 2. IntelConfEcalc (Intel-Optimized CPU Calculator)
**Location:** `src/main/cc/IntelConfEcalc/`

**Status:** Unknown Optimization Level
- Appears to be an alternative implementation or Intel-specific variant
- **Needs investigation:** What Intel-specific optimizations are present?
- Could potentially share SIMD optimizations from ConfEcalc

### 3. CudaConfEcalc (GPU Calculator)
**Location:** `src/main/cu/CudaConfEcalc/`

**Status:** Not Relevant for CPU SIMD
- GPU-accelerated using CUDA
- Already optimized for parallel execution on GPU

## High-Priority SIMD Candidates

### 1. CCD Minimization (Cyclic Coordinate Descent) - High Priority

**Location:** `src/main/cc/ConfEcalc/minimization.h`

**Current Implementation:**
- Sequential line search over degrees of freedom (DOFs)
- Hot loop: `for (int d=0; d<dofs.get_size(); d++)` (line 386)
- Each DOF requires multiple energy function evaluations

**SIMD Opportunities:**

#### A. Threading (OpenMP) - High Priority
```cpp
// Current: Sequential
for (int d=0; d<dofs.get_size(); d++) {
    next.x[d] = line_search(dofs, d, next.x[d], step);
}

// Optimized: Parallel
#pragma omp parallel for
for (int d=0; d<dofs.get_size(); d++) {
    next.x[d] = line_search(dofs, d, next.x[d], step);
}
```
**Expected Speedup:** 2-4x on 4-core CPU (DOFs are independent)

**Challenges:**
- Line search modifies `dofs[d]` state
- Need to ensure thread-safety
- May need per-thread copies of assignment state

#### B. Batch Energy Evaluation - Medium Priority
- If multiple DOFs affect disjoint atom sets, batch energy calculations
- Vectorize energy calculation across multiple DOF trial values
- **Complexity:** High - requires dependency analysis

**Code Structure:**
```cpp
template<typename T>
static void minimize_ccd(Dofs<T> & dofs, DofValues<T> & here) {
    // Iterate over DOFs sequentially
    for (int d=0; d<dofs.get_size(); d++) {
        // Line search calls energy function multiple times
        next.x[d] = line_search(dofs, d, next.x[d], step);
    }
}
```

**Energy Evaluation Pattern:**
- `dofs.eval_efunc(d, x)` - evaluates energy for single DOF
- Calls `efunc(assignment, dof.get_inters())` - already SIMD-optimized
- Multiple evaluations per DOF: `f(x)`, `f(x+step)`, `f(x-step)`, etc.

### 2. Coordinate Transformations - Medium Priority

**Location:** 
- `src/main/cc/ConfEcalc/motions/transrot.h` - TranslationRotation::apply()
- `src/main/cc/ConfEcalc/rotation.h` - Rotation matrix-vector multiply
- `src/main/cc/ConfEcalc/real3.h` - Vector operations

**Current Implementation:**
- TranslationRotation applies rotations and translations to multiple atoms
- Hot loop: `for (int i=0; i<modified_atomi.get_size(); i++)` (line 128)
- Each iteration: subtract centroid, apply rotation, apply translation, add centroid

**SIMD Implementation Status:** IN PROGRESS

**Files Created:**
- `real3_simd.h`: Vectorized Real3 operations (normalize, dot, cross, add, subtract)
- `rotation_simd.h`: Vectorized rotation matrix-vector multiply
- `transrot_simd.h`: Vectorized TranslationRotation::apply()

**SIMD Opportunities:**

#### A. Rotation Matrix-Vector Multiply (Vectorized)
- Process 4 vectors simultaneously with AVX2
- Process 8 vectors simultaneously with AVX-512
- **Expected Speedup:** 2-4x (AVX2/AVX-512)

#### B. TranslationRotation Apply Loop (Vectorized)
- Vectorize the atom transformation loop
- Process 4 atoms at a time with AVX2
- **Expected Speedup:** 2-3x

**Code Pattern:**
```cpp
// TranslationRotation::apply() - hot loop
for (int i=0; i<modified_atomi.get_size(); i++) {
    Real3<T> & p = assignment.atoms[atomi];
    p -= desc.centroid;
    p = transform_current.rotation*(p + transform_current.translation);
    p = transform_next.rotation*p + transform_next.translation;
    p += desc.centroid;
}
```

### 3. Real3 Operations - Low Priority

**Location:** `src/main/cc/ConfEcalc/real3.h`

**Current Status:** Already has SIMD-optimized `distance_sq()` (used in energy calculation)

**Additional Opportunities:**
- Cross products, dot products
- Normalization
- Vector addition/subtraction (already done in SIMD energy calc)

## Medium-Priority Candidates

### 4. Rotation Operations
**Location:** `src/main/cc/ConfEcalc/rotation.h`

**Operations:**
- Matrix-vector multiplications
- Quaternion operations
- **SIMD Benefit:** Moderate (used less frequently than energy calc)

### 5. Motion Calculations (Dihedral, TranslationRotation)
**Location:** `src/main/cc/ConfEcalc/motions/`

**Operations:**
- Trigonometric functions (sin, cos)
- Coordinate transformations
- **SIMD Benefit:** Low-Medium (depends on batching opportunities)

## Java-to-C++ Port Candidates

### EPIC Matrix (Java)
**Location:** Java codebase

**Status:** Not Yet Ported
- Dense matrix operations
- Currently in Java
- **Recommendation:** Port to C++ first, then SIMD optimize
- **Expected Speedup:** High (matrix operations are ideal for SIMD)

## Implementation Priority

### Phase 1: Complete ConfEcalc Optimization (In Progress)
- [x] Vectorize Amber pair interactions
- [x] Vectorize EEF1 pair interactions
- [x] Runtime CPU detection and dispatch
- [ ] Cache blocking for better memory access
- [ ] Pair reordering for spatial locality

### Phase 2: Threading for CCD Minimization
- [x] Add OpenMP to CCD loop
- [x] Ensure thread-safety (critical section around line_search)
- [ ] Benchmark parallel performance
- [ ] Optimize: Consider per-thread assignment copies for better parallelization

### Phase 3: Coordinate Operations
- [ ] Vectorize coordinate transformations
- [ ] Batch conformation assignments
- [ ] Optimize Real3 operations

### Phase 4: Investigation
- [ ] Analyze IntelConfEcalc differences
- [ ] Profile EPIC matrix for porting feasibility
- [ ] Identify other hot spots

## Notes

**ConfEcalc is the primary C++ component** - other directories (IntelConfEcalc, CudaConfEcalc) are:
- **IntelConfEcalc:** Alternative/Intel-specific implementation (status unclear)
- **CudaConfEcalc:** GPU version (not relevant for CPU SIMD)

**Main optimization targets:**
1. Energy calculation (DONE)
2. CCD minimization (THREADING opportunity)
3. Coordinate operations (vectorization opportunity)

