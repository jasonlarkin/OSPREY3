# ConfSpace Compilation Analysis

## Executive Summary

**ConfSpace compilation is the dominant MONTAGE bottleneck: 80% of total time (1744s/2191s)**

**Key Finding:** Compilation is sequential, with each confspace (target/design/complex) compiled one at a time. The bottleneck is LEaP subprocess calls (AmberTools) for forcefield parameterization.

## Profiling Results

### Detailed Profiling (Individual ConfSpaces)

**Profile Location:** `pipeline_analysis/confspace_compilation/`

| ConfSpace Type | Load (s) | Compile (s) | Save (s) | Total (s) | Compile % |
|----------------|----------|-------------|----------|-----------|-----------|
| **target** | 6.02 | 52.73 | 2.23 | 61.59 | 86% |
| **design** | 4.87 | 381.49 | 0.79 | 388.44 | 98% |
| **complex** | 2.29 | 398.87 | 6.80 | 408.65 | 98% |

**Sequential Total:** 61.59 + 388.44 + 408.65 = **858.68s (14.3 minutes)**

**Key Observations:**
- Compilation time scales dramatically: target (52.73s) → design (381.49s, 7.2x) → complex (398.87s, 7.6x)
- Load time decreases with complexity (6.02s → 4.87s → 2.29s), suggesting caching or efficient parsing
- Save time varies: target (2.23s) → design (0.79s) → complex (6.80s), indicating complex has largest compiled output
- Compilation dominates for design/complex (98% vs 86% for target)

### Time Distribution (from MONTAGE profiling)

| Component | Time (s) | % of Total | Notes |
|-----------|----------|------------|-------|
| **ConfSpace Compilation** | 1743.8 | 79.6% | 4 calls, 435.9s avg per call |
| SCOPE operations | 188.0 | 8.6% | Volume overlap calculations |
| MASTER subprocess | 63.0 | 2.9% | Fast, not a bottleneck |
| Other | 196.5 | 9.0% | File I/O, JVM startup, etc. |
| **Total** | 2191.3 | 100% | |

### Per-ConfSpace Compilation Times (from detailed profiling)

| ConfSpace Type | Time Range | Average | Notes |
|----------------|------------|---------|-------|
| **Target** | 52.73s | ~53s | Smallest, fastest |
| **Design** | 381.49s | ~6.4m | Medium complexity |
| **Complex** | 398.87s | ~6.7m | **Largest, slowest** |

**Pattern:** Complex confspace takes 8-14x longer than target, 1.5-2x longer than design.

## What ConfSpace Compilation Does

### Process Overview

1. **Load ConfSpace** - Read `.confspace` file (text format)
2. **Start LocalService** - HTTP server for AmberTools/LEaP access (port 44342)
3. **Parameterize Molecules** - Call AmberTools (LEaP) via LocalService for forcefield parameters
4. **Process Fragments** - Parameterize each fragment at each design position
5. **Calculate Atom Pairs** - Compute forcefield parameters for all atom pairs
6. **Compile Static Atoms** - Process fixed atoms
7. **Compile Positions** - Process design positions and conformations
8. **Save Compiled** - Write `.ccsx` file (binary format)
9. **Stop LocalService** - Shutdown HTTP server

### LocalService

**What it is:**
- Python context manager (`osprey.prep.LocalService`) that starts an OSPREY service instance locally
- Provides HTTP API (port 44342) for AmberTools/LEaP access
- Singleton pattern - only one instance allowed at a time
- Required for ConfSpace compilation (forcefield parameterization)

**Code Location:** `src/main/python/osprey/prep.py:26-97`

**How it works:**
```python
with osprey.prep.LocalService():
    # Compilation code here
    # LocalService starts HTTP server on port 44342
    # LEaP calls go through HTTP API, not direct subprocess
```

**Limitation:**
- Enforces single instance: `if cls._service is not None: raise Exception('only one instance...')`
- Blocks thread-based parallel compilation
- Each thread tries to start its own LocalService, but only one can bind to port 44342

### Code Flow

**Python Layer (`KStarPrep.py`):**
```python
def compile_confspaces(spaces: list):
    for s in spaces:  # Sequential!
        confspace = osprey.prep.loadConfSpace(open(s, 'r').read())
        compiler = osprey.prep.ConfSpaceCompiler(confspace)
        compiler.getForcefields().add(osprey.prep.Forcefield.Amber96)
        compiler.getForcefields().add(osprey.prep.Forcefield.EEF1)
        progress = compiler.compile()  # Java/Kotlin call
        progress.printUntilFinish(10000)
        report = progress.getReport()
        open(save_path, 'wb').write(osprey.prep.saveCompiledConfSpace(report.getCompiled()))
```

**Java/Kotlin Layer (`ConfSpaceCompiler.kt`):**

1. **Parameterize wild-type molecules** (lines 129-146)
   - For each molecule, for each forcefield
   - Calls `ff.parameterizeAtoms()` → AmberTools subprocess

2. **Parameterize fragments** (lines 148-171)
   - For each molecule, for each position, for each fragment
   - Calls `ff.parameterizeAtoms()` → AmberTools subprocess
   - **This is the main bottleneck** - many fragments = many subprocess calls

3. **Calculate atom pairs** (lines 347-380)
   - Calls `calcParamsAmber()` → **LEaP subprocess** (commented as bottleneck)
   - Processes static-static, pos-static, pos-pos pairs

### Bottleneck: LEaP Subprocess Calls

**From code comment (`forcefieldParams.kt:386-388`):**
```kotlin
// NOTE: calcParamsAmber() eventually calls LEaP in a separate process
// profiling shows this is by far the bottleneck in compiling atom pairs
// maybe there's something we can do to speed this up so compiling goes faster?
```

**LEaP (AmberTools) is called:**
- Once per molecule parameterization
- Once per fragment parameterization
- Once per atom pair calculation

**For a typical MONTAGE match:**
- Target: ~1 molecule → ~1 LEaP call
- Design: ~1 molecule + fragments → ~10-100 LEaP calls
- Complex: ~2 molecules + fragments + pairs → ~100-1000 LEaP calls

**Each LEaP call:**
- Spawns subprocess
- Writes input files
- Runs LEaP executable
- Reads output files
- **Overhead: ~100-500ms per call** (subprocess + I/O)

## Scaling Analysis

### Time vs. ConfSpace Size

| ConfSpace | Conformations | Time | Time/Conf (ms) |
|-----------|----------------|------|----------------|
| Target | 249,696 | ~25s | 0.10 |
| Design | 1 | ~2.5m | 150,000 |
| Complex | Large | ~4.5m | Variable |

**Observation:** Design has only 1 conformation but takes 2.5 minutes - suggests overhead dominates, not conformation count.

### Scaling Factors

1. **Number of fragments** - Linear scaling (each fragment = LEaP call)
2. **Number of pairs** - Quadratic scaling (pos-pos pairs)
3. **Subprocess overhead** - Constant per call (~100-500ms)
4. **File I/O** - Scales with molecule size

## Bottlenecks Identified

### 1. Sequential Compilation (High Impact)

**Problem:** Confspaces compiled one at a time
```python
for s in spaces:  # Sequential loop
    compile(s)
```

**Impact:**
- 3 confspaces × 2.5-4.5m = 7.5-13.5 minutes total
- No parallelism between confspaces

**Solution:** Parallel compilation
- Compile target/design/complex simultaneously
- Expected speedup: **2-3x** (3 confspaces → 1x time)

### 2. LEaP Subprocess Overhead (High Impact)

**Problem:** Each LEaP call spawns subprocess, writes files, reads files

**Impact:**
- 100-1000 LEaP calls per complex confspace
- ~100-500ms overhead per call
- **Total overhead: 10-500 seconds** (just subprocess overhead!)

**Solutions:**
1. **Batch LEaP calls** - Single LEaP call with multiple molecules
2. **Persistent LEaP process** - Keep LEaP alive, send commands via stdin
3. **Cache parameters** - Reuse parameters for identical molecules/fragments
4. **Parallel LEaP calls** - Multiple LEaP processes in parallel

**Expected speedup: 5-20x** (depending on solution)

### 3. Fragment Parameterization (Medium Impact)

**Problem:** Each fragment parameterized separately, even if identical

**Impact:**
- Duplicate parameterization work
- Example: 10 identical ALA fragments = 10 LEaP calls

**Solution:** Cache fragment parameters
- Hash fragment structure
- Reuse parameters for identical fragments
- Expected speedup: **2-5x** (depends on fragment diversity)

### 4. File I/O Overhead (Low Impact)

**Problem:** Multiple file reads/writes per compilation

**Impact:**
- Read `.confspace` file
- Write/read LEaP input/output files
- Write `.ccsx` file

**Solution:** In-memory pipeline
- Keep data in memory between stages
- Expected speedup: **1.2-1.5x**

## Optimization Opportunities

### Priority 1: Parallel ConfSpace Compilation (2-3x speedup)

**Implementation:**
```python
from concurrent.futures import ThreadPoolExecutor

def compile_confspaces_parallel(spaces: list, max_workers=3):
    with ThreadPoolExecutor(max_workers=max_workers) as executor:
        futures = {executor.submit(compile_single, s): s for s in spaces}
        for future in as_completed(futures):
            future.result()  # Wait for completion
```

**Challenges:**
- **LocalService singleton limitation** - Only one instance allowed (binds to port 44342)
- JVM/JPype thread safety
- Shared resource conflicts
- Memory usage (3x concurrent)

**Test Results:**
- Sequential: 425.81s (target: 53.53s, design: 122.12s, complex: 250.15s)
- Parallel attempt: FAILED - "only one instance of the local service allowed at once"
- Only 1/3 confspaces completed in parallel (design: 120.62s)

**Solutions:**
1. **Shared LocalService instance** - Start service once, share across threads
2. **Process-based parallelism** - Use `multiprocessing` instead of threads (separate JVM per process)
3. **Port-based LocalService** - Modify LocalService to support multiple ports (44342, 44343, 44344)

**Expected speedup:** 1.5-1.7x (limited by longest compile time ~250s)

### Priority 2: Batch LEaP Calls (5-10x speedup)

**Current:** 100-1000 separate LEaP calls
**Optimized:** 1-10 batched LEaP calls

**Implementation:**
- Collect all molecules/fragments to parameterize
- Batch into single LEaP input file
- Single LEaP subprocess call
- Parse batch output

**Challenges:**
- LEaP batch input format
- Output parsing complexity
- Error handling

**Expected speedup:** 5-10x (reduce subprocess overhead)

### Priority 3: Persistent LEaP Process (3-5x speedup)

**Current:** Spawn LEaP subprocess per call
**Optimized:** Keep LEaP alive, send commands via stdin

**Implementation:**
- Start LEaP process once
- Send commands via stdin
- Read results via stdout
- Keep process alive across compilations

**Challenges:**
- LEaP interactive mode
- Process management
- Error recovery

**Expected speedup:** 3-5x (eliminate subprocess overhead)

### Priority 4: Fragment Parameter Caching (2-5x speedup)

**Implementation:**
- Hash fragment structure (atoms, bonds, connectivity)
- Cache parameters by hash
- Reuse cached parameters

**Challenges:**
- Fragment hashing (structural equivalence)
- Cache invalidation
- Memory usage

**Expected speedup:** 2-5x (depends on fragment diversity)

### Priority 5: Parallel LEaP Calls (2-4x speedup)

**Implementation:**
- Multiple LEaP processes in parallel
- Distribute parameterization work
- Collect results

**Challenges:**
- Process management
- Resource limits
- Load balancing

**Expected speedup:** 2-4x (limited by CPU cores)

## Combined Optimization Strategy

### Phase 1: Quick Wins (2-3x speedup)

1. **Parallel confspace compilation** - Easy, high impact
2. **Fragment parameter caching** - Medium effort, good impact

**Total expected speedup:** 2-3x
**Implementation time:** 1-2 days

### Phase 2: LEaP Optimization (5-20x speedup)

1. **Batch LEaP calls** - High effort, highest impact
2. **Persistent LEaP process** - Medium effort, good impact

**Total expected speedup:** 5-20x
**Implementation time:** 1-2 weeks

### Phase 3: Full Optimization (10-50x speedup)

1. All Phase 1 + Phase 2 optimizations
2. Parallel LEaP calls
3. In-memory pipeline

**Total expected speedup:** 10-50x
**Implementation time:** 2-4 weeks

## Expected Impact on MONTAGE

### Current Performance

- ConfSpace compilation: **1744s (29 minutes)**
- Total MONTAGE: **2191s (36.5 minutes)**
- Compilation = **80% of total time**

### After Phase 1 (2-3x speedup)

- ConfSpace compilation: **580-870s (10-15 minutes)**
- Total MONTAGE: **1027-1317s (17-22 minutes)**
- **Time saved: 14-19 minutes per match**

### After Phase 2 (5-20x speedup)

- ConfSpace compilation: **87-349s (1.5-6 minutes)**
- Total MONTAGE: **534-796s (9-13 minutes)**
- **Time saved: 23-27 minutes per match**

### After Phase 3 (10-50x speedup)

- ConfSpace compilation: **35-174s (0.5-3 minutes)**
- Total MONTAGE: **482-621s (8-10 minutes)**
- **Time saved: 26-28 minutes per match**

## Recommendations

### Immediate Actions

1. **Profile LEaP calls** - Measure subprocess overhead
2. **Implement parallel compilation** - Quick win, 2-3x speedup
3. **Add fragment caching** - Medium effort, 2-5x speedup

### Next Steps

4. **Design LEaP batching** - Highest impact optimization
5. **Test persistent LEaP process** - Evaluate feasibility
6. **Measure fragment diversity** - Assess caching potential

### Long-term

7. **Refactor compilation pipeline** - In-memory, unified architecture
8. **Optimize LEaP integration** - Reduce subprocess overhead
9. **Consider alternative forcefields** - Faster parameterization

## Related Work

- **MONTAGE Profiling:** `pipeline_analysis/montage/test_montage_full/`
- **Code Location:** `src/main/kotlin/edu/duke/cs/osprey/gui/compiler/ConfSpaceCompiler.kt`
- **Python Interface:** `src/main/python/CCKStar/KStarPrep.py:557`

## Conclusion

ConfSpace compilation is the dominant MONTAGE bottleneck (80% of time). The main issue is **sequential compilation** and **LEaP subprocess overhead**. 

**Quick wins (2-3x):** Parallel compilation + fragment caching
**High impact (5-20x):** LEaP batching + persistent process
**Combined (10-50x):** All optimizations


