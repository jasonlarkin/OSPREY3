# LEaP Call Instrumentation Plan

## Goal
Understand LEaP call patterns during ConfSpace compilation to identify optimization opportunities.

## Current Understanding

### LEaP Call Flow
1. **Kotlin:** `ConfSpaceCompiler.kt` calls `ff.parameterizeAtoms()`
2. **Kotlin:** `AmberForcefieldParams.parameterizeAtoms()` calls `calcParamsAmber()`
3. **Kotlin:** `calcParamsAmber()` makes HTTP request to LocalService
4. **Kotlin:** `forcefieldParams.kt` service handler calls `Leap.run()`
5. **Kotlin:** `Leap.run()` spawns `teLeap` subprocess
6. **Subprocess:** `teLeap` executes, writes output files
7. **Kotlin:** `Leap.run()` reads output files, returns results

### Call Sites (from code analysis)
- **Wild-type molecule parameterization:** `ConfSpaceCompiler.kt:134`
- **Fragment parameterization:** `ConfSpaceCompiler.kt:157`
- **Atom pair calculation:** `forcefieldParams.kt:61` (via `calcParamsAmber()`)

## Instrumentation Options

### Option 1: Add Logging to Kotlin Code (Recommended)

**Files to modify:**
1. `src/main/kotlin/edu/duke/cs/osprey/service/amber/Leap.kt`
   - Add timing/logging to `Leap.run()`
   - Log: call type, molecule count, duration, file sizes

2. `src/main/kotlin/edu/duke/cs/osprey/gui/compiler/ConfSpaceCompiler.kt`
   - Add logging to `parameterizeAtoms()` calls
   - Log: molecule type, fragment type, call context

3. `src/main/kotlin/edu/duke/cs/osprey/gui/forcefield/amber/forcefieldParams.kt`
   - Add logging to `calcParamsAmber()`
   - Log: number of molecules, batch size, duration

**Implementation:**
```kotlin
// In Leap.kt
fun run(...): Results {
    val startTime = System.currentTimeMillis()
    val callId = callCounter.incrementAndGet()
    
    logger.info("LEaP call #$callId: ${commands.lines().firstOrNull()}")
    
    val results = // ... existing code ...
    
    val duration = System.currentTimeMillis() - startTime
    logger.info("LEaP call #$callId completed: ${duration}ms, ${filesToWrite.size} input files, ${filesToRead.size} output files")
    
    return results
}
```

**Output:** Structured logs with call counts, timing, patterns

### Option 2: JVM Profiling (async-profiler)

**Approach:** Use async-profiler to capture HTTP service calls

**Command:**
```bash
java -agentpath:/path/to/libasyncProfiler.so=start,event=cpu,alloc,file=leap_profile.html \
  -jar osprey.jar
```

**Limitations:** 
- Shows HTTP calls but not LEaP subprocess details
- Less granular than code instrumentation

### Option 3: Process Monitoring (strace/ltrace)

**Approach:** Monitor `teLeap` subprocess calls

**Command:**
```bash
strace -f -e trace=execve,clone -o leap_trace.log \
  python3 compile_confspaces.py
```

**Limitations:**
- Shows subprocess spawns but not call context
- Doesn't show which molecule/fragment is being parameterized

## Recommended Approach

**Phase 1: Quick Analysis (Current)**
- Use simple timing script (already created)
- Get basic compilation times
- Identify if LEaP is the bottleneck (confirmed: yes)

**Phase 2: Code Instrumentation**
- Add logging to `Leap.run()` in `Leap.kt`
- Log: call ID, duration, input file count, output file count
- Add logging to `ConfSpaceCompiler.kt` for context
- Run compilation, analyze logs

**Phase 3: Pattern Analysis**
- Count calls per confspace type
- Identify duplicate parameterizations (caching opportunity)
- Measure batch sizes (batching opportunity)
- Calculate subprocess overhead

## Metrics to Collect

1. **Call Count**
   - Total LEaP calls per confspace
   - Calls per molecule type
   - Calls per fragment type

2. **Timing**
   - Per-call duration
   - Subprocess overhead (spawn + I/O)
   - Total LEaP time vs total compilation time

3. **Patterns**
   - Duplicate calls (same molecule/fragment)
   - Batch sizes (molecules per call)
   - Call frequency (calls per second)

4. **Resource Usage**
   - CPU usage during LEaP calls
   - Memory usage
   - I/O patterns

## Expected Findings

Based on code analysis:
- **Target confspace:** ~1-10 LEaP calls (wild-type only)
- **Design confspace:** ~10-100 LEaP calls (wild-type + fragments)
- **Complex confspace:** ~100-1000 LEaP calls (wild-type + fragments + atom pairs)

**Hypothesis:**
- Many duplicate fragment parameterizations (caching opportunity)
- Small batch sizes (1 molecule per call) (batching opportunity)
- High subprocess overhead (100-500ms per call) (persistent process opportunity)

## Next Steps

1. **Add logging to `Leap.kt`** - Quick win, high value
2. **Run compilation with logging** - Collect data
3. **Analyze patterns** - Identify optimization targets
4. **Implement optimizations** - Batching, caching, persistent process

