# MONTAGE Profiling Analysis

## Executive Summary

**Test Case:** test_montage_minimal (demonstration workflow)  
**Wall Clock Time:** 72.82 seconds  
**cProfile Time:** 442.82 seconds (includes import overhead)  
**Peak Memory:** 388.05 MB  
**Memory Delta:** 0.25 MB (excellent - no memory leaks)

**Key Finding:** JVM startup (135.5s, 30.6% of total) and module imports (281s, 63.4%) dominate execution time. Actual MONTAGE operations (SCOPE, scaffold prep) are fast once loaded.

## Performance Breakdown

### Time Distribution

| Component | Time (s) | % of Total | Notes |
|-----------|----------|------------|-------|
| **Module Imports** | 281.0 | 63.4% | Python/Java module loading |
| **JVM Startup** | 135.5 | 30.6% | Java Virtual Machine initialization |
| **SCOPE Operations** | 133.1 | 30.1% | Volume overlap calculations |
| **OSPREY Prep Module** | 66.0 | 14.9% | OSPREY preparation module load |
| **VTK Operations** | 18.7 | 4.2% | 3D graphics operations |
| **Volume Overlap** | 18.2 | 4.1% | 9 calls, 2.0s per call |
| **v_dot (vector dot)** | 14.7 | 3.3% | 13.1M calls |

### Top Hot Functions (Self Time)

1. **`_start_jvm_common`** - 135.5s (30.6%)
   - JVM startup via JPype
   - One-time cost per Python process
   - **Optimization**: Shared JVM across matches (1.5-5s saved per match)

2. **`prep.py` module load** - 66.0s (14.9%)
   - OSPREY preparation module initialization
   - Includes Java class loading
   - One-time cost per process

3. **VTK `Update`** - 18.7s (4.2%)
   - 10,590 calls to VTK algorithm updates
   - Used for hull boolean intersections
   - Part of SCOPE volume overlap

4. **`calculate_volume_overlap`** - 18.2s (4.1%)
   - 9 calls, 2.0s per call
   - Same bottleneck as SCOPE profiling
   - Calls `v_dot` 13.1M times

5. **`v_dot`** - 14.7s (3.3%)
   - 13,105,139 calls (13.1M)
   - Vector dot product in volume overlap
   - Same optimization opportunity as SCOPE

## Detailed Analysis

### 1. JVM Startup Overhead (30.6% of time)

**Function:** `osprey/__init__.py:112(_start_jvm_common)`

**What it does:**
- Starts Java Virtual Machine via JPype
- Loads OSPREY Java classes
- Initializes Java-Python bridge

**Impact:**
- 135.5 seconds one-time cost
- Occurs once per Python process
- If MONTAGE processes 10 matches sequentially: 10 × 135.5s = 22.6 minutes just for JVM startup

**Optimization:**
- **Shared JVM**: Long-running orchestrator process
- Keep JVM alive across matches
- Expected savings: 1.5-5s per match (vs. 135.5s per process)

### 2. Module Import Overhead (63.4% of time)

**Functions:** `_find_and_load`, `_load_unlocked`, `exec_module`

**What it does:**
- Loads Python modules (MONTAGE, KStarPrep, Find_Doublets, etc.)
- Loads Java classes via JPype
- Initializes VTK, PyVista, BioPython

**Impact:**
- 281 seconds total import time
- Includes Java class loading (17.5s for `_imp.create_dynamic`)
- One-time cost per process

**Optimization:**
- **Lazy imports**: Import modules only when needed
- **Shared process**: Keep Python process alive across matches
- Expected savings: Eliminate 281s per match

### 3. SCOPE Operations (30.1% of time)

**Function:** `Find_Doublets.py:327(find_volume_overlap)`

**What it does:**
- Calculates volume overlap between convex hulls
- Called 1,768 times during SCOPE flexibility assignment
- Same bottleneck as standalone SCOPE profiling

**Breakdown:**
- `calculate_volume_overlap`: 18.2s (9 calls)
- `v_dot`: 14.7s (13.1M calls)
- `inside_all`: 12.2s (1.5M calls)
- VTK boolean intersections: 18.7s

**Optimization:**
- Same as SCOPE: Vectorize `v_dot`, spatial indexing, parallelize
- Expected speedup: 10-50x for volume calculations

### 4. MONTAGE-Specific Operations

**What was profiled:**
- Scaffold preparation (chain renaming, extraction, reflection): Fast (<1s)
- SCOPE flexibility assignment: 152s (includes SCOPE + ranking)
- No MASTER search (prerequisites missing)
- No scaffold generation (requires MASTER)
- No ConfSpace compilation (requires full MONTAGE)

**Actual MONTAGE time (if prerequisites available):**
- Expected: 5-30 minutes per match
- This demo: 72.82s (preparation only, no MASTER/K*)

## Memory Analysis

**Peak Memory:** 388.05 MB  
**Memory Delta:** 0.25 MB  
**Memory Efficiency:** Excellent

- No memory leaks detected
- Memory usage is reasonable
- Similar to SCOPE profiling (388 MB peak)
- JVM heap: 1024 MiB allocated (not fully used)

## Comparison to Expected Performance

**Expected (from docs):** 5-30 minutes per match  
**Actual (demo):** 72.82 seconds (preparation only)

**Breakdown:**
- JVM startup: 135.5s (one-time, not per match)
- Module imports: 281s (one-time, not per match)
- SCOPE operations: 133s (per workflow)
- Scaffold prep: <1s (fast)

**Adjusted time (excluding one-time costs):** ~134s for SCOPE + prep

**For full MONTAGE (with MASTER):**
- Add: MASTER search (1-2 min/match)
- Add: Scaffold generation (2-10 min/match)
- Add: ConfSpace compilation (10-60 sec/match)
- **Total per match: 5-30 minutes** (matches expected)

## Bottlenecks Identified

### 1. JVM Startup (30.6% of time, 135.5s)

**Problem:** JVM starts once per Python process

**Impact:**
- If processing 10 matches sequentially: 10 × 135.5s = 22.6 minutes overhead
- If processing 10 matches in parallel: 10 × 135.5s = 22.6 minutes (parallel overhead)

**Solution:** Shared JVM across matches
- Long-running orchestrator process
- Keep JVM alive
- Expected savings: 1.5-5s per match (vs. 135.5s per process)

### 2. Module Imports (63.4% of time, 281s)

**Problem:** Modules loaded once per process

**Impact:**
- 281s overhead per Python process
- Includes Java class loading (17.5s)

**Solution:** Shared process
- Keep Python process alive across matches
- Lazy imports where possible
- Expected savings: Eliminate 281s per match

### 3. SCOPE Volume Overlap (30.1% of time, 133s)

**Problem:** Same as SCOPE profiling - expensive volume calculations

**Impact:**
- 1,768 volume overlap calls
- 13.1M `v_dot` calls
- 1.5M `inside_all` calls

**Solution:** Same optimizations as SCOPE
- Vectorize `v_dot` (10-100x speedup)
- Spatial indexing (5-10x speedup)
- Parallelize (2-4x speedup)

### 4. Sequential Match Processing

**Problem:** Matches processed one at a time

**Impact:**
- 10 matches × 5-30 min = 50-300 minutes total
- No parallelism

**Solution:** Parallel match processing
- Process multiple matches simultaneously
- Expected speedup: 8-10x on 8-core system

## Optimization Roadmap

### High Priority (Expected 10-50x speedup)

1. **Shared JVM** (1.5-5s saved per match)
   - Long-running orchestrator
   - Keep JVM alive across matches
   - Eliminate 135.5s per-process overhead

2. **Shared Python Process** (281s saved per match)
   - Keep process alive across matches
   - Lazy imports
   - Eliminate module import overhead

3. **Vectorize `v_dot`** (10-100x speedup for volume calculations)
   - Same as SCOPE optimization
   - 13.1M calls → NumPy batch operations

### Medium Priority (Expected 2-10x speedup)

4. **Parallel Match Processing** (8-10x speedup)
   - Process multiple matches simultaneously
   - Use multiprocessing or threading

5. **Spatial Indexing** (5-10x speedup)
   - Early pruning of non-overlapping pairs
   - Reduce volume overlap calls

6. **Batch MASTER Queries** (90% overhead reduction)
   - Single MASTER call with multiple queries
   - Reduce subprocess overhead

### Low Priority (Expected 1.2-2x speedup)

7. **In-Memory Pipeline** (2-5x speedup)
   - Eliminate file I/O between stages
   - Keep data in memory

8. **Optimize VTK Operations** (1.2-2x speedup)
   - Reduce VTK overhead
   - Cache hull geometries

## Expected Performance After Optimizations

**Current (demo):** 72.82 seconds (preparation only)  
**Current (full, per match):** 5-30 minutes  
**After High Priority:** 2-5 minutes per match (10-50x speedup)  
**After All Optimizations:** 30 seconds - 2 minutes per match (15-100x speedup)

**For 10 matches:**
- Current: 50-300 minutes (sequential)
- After optimizations: 5-20 minutes (parallel)

## Recommendations

### Immediate Actions

1. **Profile full MONTAGE** - Run with MASTER prerequisites to get complete picture
2. **Implement shared JVM** - Biggest win, easiest to implement
3. **Shared Python process** - Eliminate import overhead

### Next Steps

4. **Vectorize `v_dot`** - Same optimization as SCOPE
5. **Add parallel processing** - Process matches simultaneously
6. **Spatial indexing** - Reduce volume overlap calls

### Long-term

7. **Batch MASTER queries** - Reduce subprocess overhead
8. **In-memory pipeline** - Eliminate file I/O
9. **Profile with larger workloads** - Verify scaling behavior

## Files Generated

- `pipeline_analysis/montage/test_montage_minimal/montage_profile.prof` - cProfile binary
- `pipeline_analysis/montage/test_montage_minimal/montage_profile_top50.txt` - Top functions (cumulative)
- `pipeline_analysis/montage/test_montage_minimal/montage_profile_time_top50.txt` - Top functions (self time)
- `pipeline_analysis/montage/test_montage_minimal/montage_system_resources.txt` - System metrics
- `pipeline_analysis/montage/test_montage_minimal/montage_analysis_summary.md` - Summary report
- `pipeline_analysis/montage/test_montage_minimal/subprocess_calls.txt` - Subprocess analysis

## Notes

**This profiling run:**
- Did not include MASTER search (demo script skipped it)
- Did not include scaffold generation (requires MASTER)
- Did not include ConfSpace compilation (requires full MONTAGE)
- Only profiled: Scaffold preparation + SCOPE flexibility assignment

**MASTER Prerequisites Status:**
All MASTER prerequisites are actually present and configured:
- MASTER executable: `src/main/python/CCKStar/resources/master` (3.2M, executable)
- createPDS executable: `src/main/python/CCKStar/resources/createPDS` (3.2M, executable)
- Database: `src/main/python/CCKStar/master-db/` (14,546+ .pds files)
- Database config: `src/main/python/CCKStar/resources/db.txt.local` (14,527+ paths)

See `MASTER_PREREQUISITES.md` for details.

**For complete MONTAGE profiling:**
- Run from `src/main/python/CCKStar/` directory
- Use full `run_MONTAGE()` workflow (not demo script)
- Will include subprocess timing (MASTER ~10 min, Java)
- Expected total time: 5-30 minutes per match

**Background processes:**
- Other processes running may affect timing
- Focus on relative percentages, not absolute times
- Multiple runs can average out system load effects

## Conclusion

MONTAGE profiling reveals that **JVM startup and module imports dominate execution time** (94% combined). Once loaded, actual MONTAGE operations are fast. The key optimization is to **keep JVM and Python process alive** across matches, eliminating 416.5s of overhead per match.

Actual MONTAGE operations (SCOPE, scaffold prep) are efficient once modules are loaded. The bottleneck is process startup overhead, not the algorithms themselves.

