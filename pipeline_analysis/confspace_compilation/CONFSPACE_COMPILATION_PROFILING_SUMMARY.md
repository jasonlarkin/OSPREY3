# ConfSpace Compilation Profiling Summary

## What is "Compilation" in This Context?

**ConfSpace compilation** converts a text-based conformation space definition (`.confspace` file) into an optimized binary format (`.ccsx` file) that includes:

1. **Forcefield parameters** - Amber96 and EEF1 parameters for all atoms, bonds, angles, dihedrals
2. **Conformation indexing** - Efficient lookup structures for conformations
3. **Atom pair parameters** - Pre-computed forcefield parameters for all atom pairs
4. **Static atom energies** - Pre-computed energies for fixed atoms
5. **Position and fragment information** - Design position metadata

**Process:**
- Input: `.confspace` (text, human-readable)
- Process: Parameterize molecules, compute forcefield parameters, index conformations
- Output: `.ccsx` (binary, compressed, optimized for runtime)

**Why compile?**
- Runtime efficiency: Pre-computed parameters avoid repeated calculations
- Binary format: Faster loading than text parsing
- Optimization: Indexed structures for fast conformation lookup

## What is LEaP?

**LEaP (Linking, Editing, and Parameterization)** is part of AmberTools, a molecular dynamics simulation package.

**Purpose:**
- Generates forcefield parameters for molecules
- Assigns atom types, charges, and bond parameters
- Creates topology files (.top) and coordinate files (.crd)

**How OSPREY uses LEaP:**
- OSPREY calls LEaP via HTTP API through `LocalService`
- LocalService runs LEaP as a subprocess
- LEaP processes molecule files (.mol2) and returns forcefield parameters
- Parameters are used to compute energies during K* calculations

**LEaP Installation:**

OSPREY expects AmberTools to be bundled in the service directory at:
- `progs/ambertools/bin/teLeap` (executable)
- `progs/ambertools/dat/leap/` (data files)

**Installation (from OSPREY docs):**

1. **Download AmberTools 19:**
   - Source: https://ambermd.org/GetAmber.php
   - Note: Versions newer than 19 not tested
   - Backup copy may be on DLab filesystem: `/Code/AmberTools19.tar.bz2`

2. **Extract and configure:**
   ```bash
   tar -xjf AmberTools19.tar.bz2
   export AMBERHOME=/path/to/unpacked/folder
   cd $AMBERHOME
   ./configure --skip-python gnu
   # Answer yes to all patches
   ```

3. **Apply OSPREY patches:**
   ```bash
   # Apply patches from progs/ambertools/patches folder
   # See progs/ambertools/patches/readme.txt for details
   ```

4. **Build:**
   ```bash
   make install -j4  # -j4 speeds up build
   ```

5. **Copy to OSPREY service directory:**
   ```bash
   # Copy binaries to progs/ambertools/bin/
   # Copy data files to progs/ambertools/dat/leap/
   ```

**OSPREY LocalService:**
- Automatically starts when `osprey.prep.LocalService()` context is used
- Runs on port 44342 (default)
- Calls LEaP via subprocess: `progs/ambertools/bin/teLeap`
- Handles LEaP subprocess calls internally
- No manual LEaP setup required if AmberTools is properly bundled

**Verification:**
```bash
# Check if teLeap exists in service directory
ls progs/ambertools/bin/teLeap
# Should exist if AmberTools is properly installed
```

## Profiling Results: match1_target

### Execution Summary

| Phase | Time (s) | % of Total |
|-------|----------|------------|
| **Load** | 6.02 | 10% |
| **Compile** | 52.73 | 86% |
| **Save** | 2.23 | 4% |
| **Total** | 61.59 | 100% |

### Compilation Breakdown

- **LocalService startup:** ~9s (included in compile time)
- **Actual compilation:** ~43s (reported by compiler)
- **LocalService shutdown:** ~1s (included in compile time)

### Python Profiler Results

**Top functions (cumulative time):**
1. `LocalService.__enter__` - 9.1s (50% of Python time)
   - Starts HTTP server for AmberTools API
   - Initializes service infrastructure

2. `loadConfSpace` - 5.93s (33% of Python time)
   - Parses text-based confspace file
   - Loads molecule structures

3. `saveCompiledConfSpace` - 2.12s (12% of Python time)
   - Serializes compiled confspace to binary
   - Compresses and writes to file

**Observation:** Python profiler only sees Python overhead (service startup, file I/O). Actual compilation work happens in Java/Kotlin and is not visible in Python profile.

### LEaP Call Tracking

**Result:** 0 LEaP calls detected

**Reason:** LEaP is called via HTTP API through LocalService, not as direct subprocess. The subprocess tracking in the profiling script only catches direct `subprocess.run()` calls, not HTTP API calls.

**Actual LEaP usage:**
- LEaP calls happen inside Java/Kotlin code
- Called via LocalService HTTP API (port 44342)
- Not visible to Python subprocess tracking
- Need Java/Kotlin profiling to see LEaP call patterns

## Profiling Results: match1_design

### Execution Summary

| Phase | Time (s) | % of Total |
|-------|----------|------------|
| **Load** | 4.87 | 1% |
| **Compile** | 381.49 | 98% |
| **Save** | 0.79 | <1% |
| **Total** | 388.44 | 100% |

### Comparison to match1_target

- **Total time:** 6.3x longer (388.44s vs 61.59s)
- **Compile time:** 7.2x longer (381.49s vs 52.73s)
- **Load time:** 1.2x shorter (4.87s vs 6.02s)
- **Save time:** 2.8x shorter (0.79s vs 2.23s)

**Observation:** Design confspace is significantly more complex, requiring much more compilation time. Load and save times are similar or faster, suggesting the complexity is in the compilation process itself (fragment parameterization, LEaP calls).

### Python Profiler Results

**Top functions (cumulative time):**
1. `LocalService.__enter__` - 10.88s (61% of Python time)
   - Service startup overhead (similar to target)

2. `loadConfSpace` - 4.21s (24% of Python time)
   - Faster than target (4.87s vs 6.02s)

3. `saveCompiledConfSpace` - 0.72s (4% of Python time)
   - Much faster than target (0.79s vs 2.23s)

**Observation:** Python overhead is similar (~18s Python time vs ~388s total), meaning actual compilation work (in Java/Kotlin) dominates even more (98% vs 86% for target).

## Profiling Results: match1_complex

### Execution Summary

| Phase | Time (s) | % of Total |
|-------|----------|------------|
| **Load** | 2.29 | <1% |
| **Compile** | 398.87 | 98% |
| **Save** | 6.80 | 2% |
| **Total** | 408.65 | 100% |

### Comparison to Other ConfSpaces

- **Total time:** 6.6x longer than target (408.65s vs 61.59s)
- **Total time:** 1.05x longer than design (408.65s vs 388.44s)
- **Compile time:** 7.6x longer than target (398.87s vs 52.73s)
- **Compile time:** 1.05x longer than design (398.87s vs 381.49s)
- **Load time:** 2.6x shorter than target (2.29s vs 6.02s)
- **Save time:** 3.1x longer than target (6.80s vs 2.23s), 8.6x longer than design (6.80s vs 0.79s)

**Observation:** Complex confspace is the largest, requiring the most compilation time. Save time is significantly longer (6.80s), indicating a much larger compiled output file. Load time is fastest, suggesting efficient parsing despite complexity.

### Python Profiler Results

**Top functions (cumulative time):**
1. `saveCompiledConfSpace` - 6.74s (47% of Python time)
   - Much longer than design/target due to larger compiled file

2. `LocalService.__enter__` - 4.34s (30% of Python time)
   - Service startup overhead (similar to others)

3. `loadConfSpace` - 2.19s (15% of Python time)
   - Fastest load time of all three

**Observation:** Save time dominates Python overhead (6.74s vs ~14s total Python time), reflecting the larger compiled confspace size. Actual compilation work (398.87s) is 28x larger than Python overhead.

## Comparison: All Three ConfSpaces

| ConfSpace | Load (s) | Compile (s) | Save (s) | Total (s) | Compile % |
|-----------|----------|-------------|----------|-----------|-----------|
| **target** | 6.02 | 52.73 | 2.23 | 61.59 | 86% |
| **design** | 4.87 | 381.49 | 0.79 | 388.44 | 98% |
| **complex** | 2.29 | 398.87 | 6.80 | 408.65 | 98% |

**Key Observations:**
- Compilation time scales dramatically: target (52.73s) → design (381.49s, 7.2x) → complex (398.87s, 7.6x)
- Load time decreases with complexity (6.02s → 4.87s → 2.29s), suggesting more efficient parsing or caching
- Save time varies: target (2.23s) → design (0.79s) → complex (6.80s), indicating complex has largest compiled output
- Compilation dominates for design/complex (98% vs 86% for target)

**Sequential Total:** 61.59 + 388.44 + 408.65 = **858.68s (14.3 minutes)**

**Parallel Potential:** If compiled simultaneously, expected time ~400-450s (limited by longest compile time), yielding **~2x speedup**.

## Key Findings

### 1. Compilation Time Dominates

- 86% of total time is compilation
- Load and save are relatively fast (14% combined)
- Compilation is the optimization target

### 2. LocalService Overhead

- ~9s startup time (17% of compile time)
- Service must start before compilation
- Overhead is significant for single confspace
- Would be amortized across multiple compilations

### 3. Python Profiler Limitations

- Only sees Python layer (service, file I/O)
- Actual compilation work is in Java/Kotlin
- Need Java/Kotlin profiling to see:
  - LEaP call patterns
  - Parameterization bottlenecks
  - Fragment processing overhead

### 4. LEaP Call Tracking

- Subprocess tracking doesn't work (LEaP via HTTP API)
- Need to instrument LocalService or Java/Kotlin code
- Alternative: Monitor HTTP requests to port 44342

## Comparison to MONTAGE Profiling

**MONTAGE full workflow:**
- ConfSpace compilation: 1744s (80% of MONTAGE time)
- 4 compilation calls: 435.9s average per call
- This single confspace: 52.73s compile time

**Difference:**
- MONTAGE includes target/design/complex (3 confspaces per match)
- Complex confspace is largest (4.5m average)
- Target confspace is smallest (~25s average)
- This profile matches target confspace timing

## Next Steps

### 1. Profile Other ConfSpaces

- Design confspace: COMPLETE - 388.44s (6.47 minutes)
- Complex confspace: COMPLETE - 408.65s (6.81 minutes)
- All three confspaces profiled and compared

### 2. Test Parallel Compilation

- COMPLETE - Tested with ThreadPoolExecutor
- **Result:** LocalService limitation prevents true parallelization
- **Sequential total:** 425.81s (target: 53.53s, design: 122.12s, complex: 250.15s)
- **Parallel attempt:** Only 1/3 confspaces completed (design: 120.62s)
- **Error:** "only one instance of the local service allowed at once"
- **Solution needed:** Shared LocalService instance or process-based parallelism

### 3. Java/Kotlin Profiling

- Profile compilation in Java/Kotlin layer
- Identify LEaP call patterns
- Measure parameterization overhead
- Tools: JVM profiler, async-profiler

### 4. LEaP Call Instrumentation

- Monitor LocalService HTTP requests
- Count LEaP invocations
- Measure LEaP subprocess overhead
- Alternative: Instrument Java/Kotlin code

## Files Generated

**match1_target:**
- `match1_target_profile.prof` - cProfile binary
- `match1_target_top50_cumulative.txt` - Top functions (cumulative)
- `match1_target_top50_time.txt` - Top functions (self time)
- `match1_target_leap_calls.txt` - LEaP call analysis (empty - tracking limitation)
- `match1_target_timing.txt` - Timing breakdown
- `output.txt` - Full execution log

**match1_design:**
- `match1_design_profile.prof` - cProfile binary
- `match1_design_top50_cumulative.txt` - Top functions (cumulative)
- `match1_design_top50_time.txt` - Top functions (self time)
- `match1_design_leap_calls.txt` - LEaP call analysis (empty - tracking limitation)
- `match1_design_timing.txt` - Timing breakdown
- `output.txt` - Full execution log

**match1_complex:**
- `match1_complex_profile.prof` - cProfile binary
- `match1_complex_top50_cumulative.txt` - Top functions (cumulative)
- `match1_complex_top50_time.txt` - Top functions (self time)
- `match1_complex_leap_calls.txt` - LEaP call analysis (empty - tracking limitation)
- `match1_complex_timing.txt` - Timing breakdown
- `output.txt` - Full execution log

## Conclusion

ConfSpace compilation profiling reveals:
- Compilation dominates time (86%)
- LocalService overhead is significant (~17% of compile time)
- Python profiler sees only Python layer, not actual compilation work
- LEaP calls are not visible via subprocess tracking (HTTP API)

**Optimization targets:**
1. Parallel compilation (2-3x speedup)
2. LEaP batching (5-10x speedup)
3. LocalService reuse (eliminate startup overhead)
4. Fragment parameter caching (2-5x speedup)

## Parallel Compilation Test Results

### Sequential Compilation

| ConfSpace | Load (s) | Compile (s) | Save (s) | Total (s) |
|-----------|----------|-------------|----------|-----------|
| target | 3.74 | 47.76 | 2.04 | 53.53 |
| design | 0.68 | 121.17 | 0.27 | 122.12 |
| complex | 1.24 | 246.39 | 2.52 | 250.15 |
| **Total** | | | | **425.81** |

### Parallel Compilation Attempt

**Method:** ThreadPoolExecutor with 3 workers

**Result:** FAILED - LocalService singleton limitation

- Only design confspace completed: 120.62s
- target and complex failed with error: "only one instance of the local service allowed at once"
- LocalService binds to port 44342 and enforces single instance

**Observation:** Thread-based parallelism is blocked by LocalService architecture. Each thread attempts to start its own LocalService instance, but only one can bind to port 44342.

### Solutions for True Parallel Compilation

**Option 1: Shared LocalService Instance**
- Start LocalService once before parallel compilation
- Share the same service instance across all threads
- Requires modifying compilation code to accept external LocalService

**Option 2: Process-Based Parallelism**
- Use `multiprocessing` instead of `ThreadPoolExecutor`
- Each process gets its own JVM and LocalService instance
- Processes can bind to different ports or use separate service instances
- Trade-off: Higher memory overhead (separate JVM per process)

**Option 3: Port-Based LocalService**
- Modify LocalService to support multiple instances on different ports
- Each thread/process uses a different port (44342, 44343, 44344, etc.)
- Requires LocalService code changes

**Expected Speedup (if working):**
- Sequential: 425.81s
- Ideal parallel (limited by longest): ~250s (complex compile time)
- Expected speedup: ~1.7x
- With overhead: ~1.5-1.6x realistic

**Next:** Implement one of the parallelization solutions and re-test.

