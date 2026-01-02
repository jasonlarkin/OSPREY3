# OSPREY Pipeline Profiling Synthesis

## Executive Summary

This document synthesizes profiling results across all OSPREY pipeline stages to identify common patterns, bottlenecks, and optimization opportunities.

**Key Finding:** Each stage has distinct bottlenecks, but common themes emerge: subprocess overhead, sequential processing, and computational hotspots that can be optimized.

## Stage-by-Stage Comparison

### Stage 1: SCOPE (Convex Hull Analysis)

**Profile Location:** `pipeline_analysis/scope/`

| Metric | Value | Notes |
|--------|-------|-------|
| **Total Time** | 208.3s (3.5 min) | Wall clock time |
| **Peak Memory** | 388 MB | Low memory usage |
| **Main Bottleneck** | Volume overlap (79.6%) | `calculate_volume_overlap` |
| **Hot Function** | `v_dot` (17.4%) | 268M calls, pure Python |
| **Scaling** | O(n²) pairs | Quadratic with residues |

**Bottlenecks:**
1. **Volume overlap calculation** - 79.6% of time
   - `calculate_volume_overlap`: 174.8s (26.5%)
   - `v_dot`: 114.6s (17.4%), 268M calls
   - `inside_all`: 90.3s (13.7%), 40M calls
2. **Module imports** - 15.3% (VTK/PyVista)
3. **Hull generation** - 6.6%

**Optimization Potential:**
- Vectorize `v_dot`: 10-100x speedup
- Spatial indexing: 5-10x speedup
- Parallelize pairs: 2-4x speedup
- **Total potential: 20-400x** (theoretical)

### Stage 2: MONTAGE (Scaffold Generation)

**Profile Location:** `pipeline_analysis/montage/test_montage_full/`

| Metric | Value | Notes |
|--------|-------|-------|
| **Total Time** | 2191.3s (36.5 min) | Full workflow with MASTER |
| **Main Bottleneck** | ConfSpace compilation (79.6%) | 1744s total |
| **MASTER Time** | 63.0s (2.9%) | Fast, not a bottleneck |
| **SCOPE Time** | 188.0s (8.6%) | Volume overlap |

**Bottlenecks:**
1. **ConfSpace compilation** - 79.6% (1744s)
   - Sequential compilation (target/design/complex)
   - LEaP subprocess calls via LocalService (100-1000 per complex)
   - Fragment parameterization overhead
   - **LocalService singleton blocks parallelization**
2. **SCOPE operations** - 8.6% (188s)
   - Same bottlenecks as Stage 1
3. **MASTER subprocess** - 2.9% (63s)
   - Fast, not a bottleneck

**Detailed ConfSpace Times (from individual profiling):**
- Target: 52.73s compile (61.59s total)
- Design: 381.49s compile (388.44s total) - 7.2x longer
- Complex: 398.87s compile (408.65s total) - 7.6x longer
- Sequential total: 858.68s (14.3 minutes)

**LocalService Limitation:**
- Python context manager that starts HTTP server (port 44342) for AmberTools/LEaP
- Singleton pattern - only one instance allowed
- Blocks thread-based parallel compilation
- Parallel test: FAILED - "only one instance of the local service allowed at once"
- Solutions: Shared service instance, process-based parallelism, or multi-port support

**Optimization Potential:**
- Parallel compilation: 1.5-1.7x speedup (blocked by LocalService, needs fix)
- Batch LEaP calls: 5-10x speedup
- Persistent LEaP process: 3-5x speedup
- Fragment caching: 2-5x speedup
- **Total potential: 10-50x** (after LocalService fix)

### Stage 3: K* Execution (Partition Function)

**Profile Location:** `pipeline_analysis/full_kstar/`

| Metric | Value | Notes |
|--------|-------|-------|
| **Total Time** | 79-170s | Varies by system size |
| **GC Overhead** | 0.51-3.40% | Low, not a bottleneck |
| **Main Bottleneck** | CPU computation | C++ native code |
| **Memory** | 168-190 MB peak | Stable, no leaks |

**Bottlenecks:**
1. **CPU computation** - C++ native code (not visible to JVM profiler)
   - Energy calculations (Amber/EEF1)
   - A* search tree construction
   - Partition function computation
2. **Sequential sequence processing** - Each sequence processed one at a time
3. **Memory allocation** - Per-sequence churn (but GC overhead is low)

**GC Analysis:**
- test2RL0: 0.51% GC overhead (excellent)
- test1GUA11: 3.40% GC overhead (acceptable)
- No Full GC events (heap sizing appropriate)
- Large pauses are system-level, not GC issues

**Optimization Potential:**
- Parallel sequence processing: 10-100x speedup
- SIMD energy calculations: 4-8x speedup
- Arena allocation: 2-5x speedup (supporting optimization)
- **Total potential: 20-400x** (theoretical, depends on core count)

### Stage 4: ARISE (Iterative Design)

**Profile Location:** `pipeline_analysis/arise/`

| Metric | Value | Notes |
|--------|-------|-------|
| **Total Time** | 23.2s | Short test case |
| **Module Imports** | 21.7s (93%) | Dominates execution |
| **Actual Processing** | <2s | Fast once loaded |
| **Memory** | Low | No issues observed |

**Bottlenecks:**
1. **Module imports** - 93% of time
   - `osprey/prep.py`: 7.4s
   - JVM startup: 3.3s
   - VTK/pyvista: 5.3s
2. **Blocked by K*** - Must wait for K* completion
3. **SCOPE re-runs** - Each iteration re-computes graph

**Optimization Potential:**
- Shared JVM/Python process: Eliminate import overhead
- Cache SCOPE results: Avoid re-computation
- **Total potential: 10-20x** (eliminate overhead)

## Cross-Stage Patterns

### 1. Subprocess Overhead (High Impact)

**Occurs in:**
- MONTAGE: LEaP subprocess calls (100-1000 per complex confspace)
- MONTAGE: MASTER subprocess (fast, but still overhead)
- SCOPE: VTK operations (subprocess-like overhead)

**Impact:**
- MONTAGE: 10-500 seconds of subprocess overhead
- Each subprocess: ~100-500ms overhead (spawn + I/O)

**Solution:**
- Batch subprocess calls
- Persistent processes (keep alive, send commands)
- Parallel subprocess execution

### 2. Sequential Processing (High Impact)

**Occurs in:**
- MONTAGE: Confspaces compiled sequentially
- K*: Sequences processed sequentially
- ARISE: Iterations processed sequentially

**Impact:**
- MONTAGE: 3 confspaces × 2.5-4.5m = 7.5-13.5 minutes
- K*: 100 sequences × 20 min = 33 hours (1.4 days)

**Solution:**
- Parallel processing (threads, processes, MPI)
- Independent work units can be parallelized

### 3. Module Import Overhead (Medium Impact)

**Occurs in:**
- All stages: Python/Java module loading
- MONTAGE: 63.4% of time (281s)
- ARISE: 93% of time (21.7s)

**Impact:**
- One-time cost per process
- If processing multiple matches: N × import_time

**Solution:**
- Shared JVM/Python process
- Lazy imports
- Keep process alive across matches

### 4. Computational Hotspots (Medium Impact)

**Occurs in:**
- SCOPE: `v_dot` (268M calls, pure Python)
- SCOPE: `inside_all` (40M calls)
- MONTAGE: Same SCOPE bottlenecks
- K*: C++ energy calculations (not visible to JVM profiler)

**Impact:**
- SCOPE: 79.6% of time in volume overlap
- K*: CPU-bound computation

**Solution:**
- Vectorization (NumPy, SIMD)
- Algorithm improvements
- Parallelization

### 5. File I/O Overhead (Low Impact)

**Occurs in:**
- All stages: Multi-process architecture
- MONTAGE: Confspace file reads/writes
- K*: Result file writes
- ARISE: K* result file reads

**Impact:**
- Visible but not dominant
- Multi-process communication via files

**Solution:**
- In-memory pipeline
- Shared memory
- Reduce file I/O

## Time Distribution Summary

| Stage | Time | % of Pipeline | Main Bottleneck | Optimization Potential |
|-------|------|---------------|-----------------|------------------------|
| **SCOPE** | 3.5 min | <1% | Volume overlap (79.6%) | 20-400x |
| **MONTAGE** | 36.5 min | ~2% | ConfSpace compilation (80%) | 10-50x |
| **K*** | 1-2 days | **96%** | Sequential sequences | 20-400x |
| **ARISE** | Minutes-hours | <1% | Blocked by K* | 10-20x |

**Note:** K* dominates total pipeline time (1-2 days for medium systems).

## Memory Patterns

### Memory Usage by Stage

| Stage | Peak Memory | Memory Delta | Notes |
|-------|-------------|--------------|-------|
| SCOPE | 388 MB | 0.12 MB | Excellent, no leaks |
| MONTAGE | 388 MB | 0.25 MB | Excellent, no leaks |
| K* | 168-190 MB | Stable | Low GC overhead |
| ARISE | Low | Stable | No issues |

**Observation:** Memory usage is reasonable across all stages. No memory leaks detected.

### GC Behavior (K*)

- **GC Overhead:** 0.51-3.40% (low, not a bottleneck)
- **Full GC:** None (heap sizing appropriate)
- **Large Pauses:** System-level, not GC issues
- **Memory Growth:** Steady, no leaks

**Conclusion:** Memory is not a primary bottleneck. Focus should be on CPU optimization.

## Common Optimization Strategies

### 1. Parallelization (Highest Impact)

**Applicable to:**
- MONTAGE: Parallel confspace compilation (2-3x)
- K*: Parallel sequence processing (10-100x)
- SCOPE: Parallel pair processing (2-4x)

**Implementation:**
- Thread pools
- Process pools
- MPI for multi-node

**Expected Impact:** 2-100x depending on stage and core count

### 2. Subprocess Optimization (High Impact)

**Applicable to:**
- MONTAGE: LEaP subprocess calls
- MONTAGE: MASTER subprocess

**Implementation:**
- Batch subprocess calls
- Persistent processes
- Parallel subprocess execution

**Expected Impact:** 5-20x for MONTAGE

### 3. Vectorization (High Impact)

**Applicable to:**
- SCOPE: `v_dot` (268M calls)
- SCOPE: `inside_all` (40M calls)
- K*: Energy calculations (C++)

**Implementation:**
- NumPy for Python
- SIMD for C++
- GPU for large batches

**Expected Impact:** 10-100x for computational hotspots

### 4. Process/Module Reuse (Medium Impact)

**Applicable to:**
- All stages: JVM startup
- All stages: Module imports
- MONTAGE: Python process

**Implementation:**
- Shared JVM across matches
- Shared Python process
- Lazy imports

**Expected Impact:** 10-20x (eliminate overhead)

### 5. Caching (Medium Impact)

**Applicable to:**
- MONTAGE: Fragment parameters
- ARISE: SCOPE results
- K*: Energy matrices (if applicable)

**Implementation:**
- Hash-based caching
- In-memory cache
- Persistent cache

**Expected Impact:** 2-5x depending on cache hit rate

## Bottleneck Priority Matrix

| Bottleneck | Stage | Impact | Effort | Priority |
|------------|-------|--------|--------|----------|
| **Sequential K* sequences** | K* | Very High | Medium | **P0** |
| **ConfSpace compilation** | MONTAGE | High | Medium | **P1** |
| **Volume overlap (`v_dot`)** | SCOPE | Medium | Low | **P2** |
| **LEaP subprocess overhead** | MONTAGE | High | High | **P2** |
| **Module import overhead** | All | Medium | Low | **P3** |
| **Sequential confspace compilation** | MONTAGE | Medium | Low | **P3** |

## Recommendations

### Immediate Actions (P0)

1. **Parallelize K* sequence processing**
   - Highest impact (10-100x speedup)
   - Addresses dominant bottleneck (96% of pipeline time)
   - Independent sequences = perfect for parallelization

### Short-term (P1-P2)

2. **Optimize ConfSpace compilation**
   - Parallel compilation (2-3x)
   - LEaP batching (5-10x)
   - Fragment caching (2-5x)

3. **Vectorize SCOPE operations**
   - `v_dot` vectorization (10-100x)
   - Spatial indexing (5-10x)

### Medium-term (P3)

4. **Reduce process overhead**
   - Shared JVM/Python process
   - Lazy imports
   - Process reuse

5. **Optimize subprocess calls**
   - Batch LEaP calls
   - Persistent processes

## Expected Overall Impact

### Current Pipeline Time (Medium System)

- SCOPE: 3.5 min
- MONTAGE: 36.5 min
- K*: 1-2 days (dominant)
- ARISE: Minutes-hours (blocked by K*)
- **Total: 1-2 days**

### After P0 Optimization (Parallel K*)

- SCOPE: 3.5 min
- MONTAGE: 36.5 min
- K*: 10-20 min (20-100x speedup)
- ARISE: Minutes-hours
- **Total: ~1 hour** (20-50x speedup)

### After All Optimizations

- SCOPE: 0.1-0.2 min (20x speedup)
- MONTAGE: 1-4 min (10-50x speedup)
- K*: 10-20 min (20-100x speedup)
- ARISE: Minutes (10-20x speedup)
- **Total: 15-30 min** (50-200x speedup)

## Conclusion

**Key Findings:**
1. **K* is the dominant bottleneck** - 96% of pipeline time
2. **ConfSpace compilation is MONTAGE bottleneck** - 80% of MONTAGE time
3. **Volume overlap is SCOPE bottleneck** - 80% of SCOPE time
4. **Memory is not a bottleneck** - Low GC overhead, no leaks
5. **Common patterns:** Sequential processing, subprocess overhead, computational hotspots

**Optimization Strategy:**
1. **Focus on K* parallelization** (P0) - Highest impact
2. **Optimize ConfSpace compilation** (P1) - High impact, medium effort
3. **Vectorize computational hotspots** (P2) - High impact, low effort
4. **Reduce process overhead** (P3) - Medium impact, low effort

**Expected Result:** 50-200x total pipeline speedup, reducing 1-2 days to 15-30 minutes.

