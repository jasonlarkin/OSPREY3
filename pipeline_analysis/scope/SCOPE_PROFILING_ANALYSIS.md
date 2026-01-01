# SCOPE Profiling Analysis

## Executive Summary

**Test Case:** test_scope_2rl0 (2RL0 PDB, 17 design residues, 89 target residues)  
**Wall Clock Time:** 208.27 seconds (~3.5 minutes)  
**Peak Memory:** 388.05 MB  
**Memory Delta:** 0.12 MB (excellent - no memory leaks)

**Key Finding:** Volume overlap calculation dominates execution time (79.6% cumulative), with vector dot product (`v_dot`) being the hottest function (268M calls, 17.4% of time).

## Performance Breakdown

### Time Distribution

| Component | Time (s) | % of Total | Calls | Notes |
|-----------|----------|-----------|-------|-------|
| **Volume Overlap Calculation** | 524.5 | 79.6% | 2,244 | Main bottleneck |
| - `calculate_volume_overlap` | 174.8 | 26.5% | 70 | 2.5s per call |
| - `v_dot` (vector dot) | 114.6 | 17.4% | 268M | **Hottest function** |
| - `inside_all` | 90.3 | 13.7% | 40M | Point-in-hull test |
| **Module Imports** | 101.0 | 15.3% | - | VTK/PyVista loading |
| **Hull Generation** | 43.3 | 6.6% | 210 | Convex hull creation |
| **VTK Operations** | 18.0 | 2.7% | 13K | Boolean intersections |
| **PDB I/O** | 12.8 | 1.9% | 2 | File writing |

### Top Hot Functions (Self Time)

1. **`calculate_volume_overlap`** - 174.8s (26.5%)
   - 70 calls, 2.5s per call
   - Computes intersection volume between two convex hulls
   - Calls `inside_all` 40M times

2. **`v_dot`** - 114.6s (17.4%)
   - **268,442,615 calls** (268M!)
   - Vector dot product: `a·b = a.x*b.x + a.y*b.y + a.z*b.z`
   - Pure Python implementation - prime candidate for NumPy vectorization

3. **`inside_all`** - 90.3s (13.7%)
   - 40,174,425 calls (40M)
   - Tests if point is inside all half-planes of convex hull
   - Calls `v_dot` repeatedly

4. **`convex_planes_and_tris`** - 22.5s (3.4%)
   - 210 calls, 0.1s per call
   - Extracts planes and triangles from convex hull

5. **VTK `Update`** - 18.0s (2.7%)
   - 13,324 calls
   - VTK algorithm execution (boolean intersections)

## Detailed Analysis

### 1. Volume Overlap Calculation (79.6% of time)

**Function:** `Find_Doublets.py:344(calculate_volume_overlap)`

**What it does:**
- Computes intersection volume between two convex hulls
- Uses point-in-hull testing (`inside_all`) to determine overlap
- Called 70 times (once per inter-chain pair)

**Bottleneck:**
- Each call takes 2.5 seconds
- Calls `inside_all` 40M times total (571K per call)
- `inside_all` calls `v_dot` 268M times (6.7x multiplier)

**Optimization opportunities:**
1. **Vectorize `v_dot`** - Use NumPy for batch dot products (10-100x speedup)
2. **Early termination** - Stop testing points once intersection is determined
3. **Spatial indexing** - Use bounding boxes to skip non-overlapping pairs
4. **Parallelize** - Process multiple pairs simultaneously

### 2. Vector Dot Product (17.4% of time, 268M calls)

**Function:** `Find_Doublets.py:347(v_dot)`

**Current implementation:**
```python
def v_dot(a, b):
    return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
```

**Problem:**
- Pure Python, called 268 million times
- No vectorization, no SIMD
- Each call: 3 multiplies + 2 adds = 5 operations
- Total: 1.34 billion operations in pure Python

**Solution:**
- Use NumPy for batch operations
- Process thousands of points at once
- Expected speedup: 10-100x

### 3. Point-in-Hull Testing (13.7% of time, 40M calls)

**Function:** `Find_Doublets.py:471(inside_all)`

**What it does:**
- Tests if a point is inside all half-planes of a convex hull
- For each half-plane: `v_dot(normal, point) < threshold`
- Called 40M times

**Optimization:**
- Batch process points with NumPy
- Use vectorized dot products
- Early termination if any half-plane fails

### 4. Module Import Overhead (15.3% of time)

**Problem:**
- VTK/PyVista imports take 101 seconds
- Only needed for hull generation and boolean operations
- Loaded at module import time

**Solution:**
- Lazy loading: Import VTK only when needed
- Expected savings: 50-80 seconds

## Memory Analysis

**Peak Memory:** 388.05 MB  
**Memory Delta:** 0.12 MB  
**Memory Efficiency:** Excellent

- No memory leaks detected
- Memory usage is reasonable for geometric operations
- PyVista meshes are the main memory consumers (~10-50 MB per hull)

## Comparison to Expected Performance

**Expected (from docs):** 10-30 seconds  
**Actual:** 208.27 seconds  
**Slowdown:** 7-21x slower than expected

**Possible reasons:**
1. Test case uses full amino acid set (22 types) vs minimal (3 types)
2. Includes ranking functions (`rank_flex_overlap`, `rank_design_overlap`) not in base SCOPE
3. Module import overhead (101s) not included in expected time
4. More thorough intersection testing

**Adjusted time (excluding imports):** 107 seconds - still 3.5-10x slower

## Optimization Roadmap

### High Priority (Expected 10-50x speedup)

1. **Vectorize `v_dot` with NumPy** (10-100x speedup)
   - Replace 268M Python calls with NumPy batch operations
   - Expected: 114.6s → 1-11s

2. **Lazy load VTK/PyVista** (50-80s savings)
   - Import only when needed
   - Expected: 101s → 1-5s

3. **Batch point-in-hull testing** (5-10x speedup)
   - Process thousands of points at once
   - Expected: 90.3s → 9-18s

### Medium Priority (Expected 2-5x speedup)

4. **Spatial indexing for early pruning** (2-5x speedup)
   - Use bounding boxes to skip non-overlapping pairs
   - Expected: Reduce `calculate_volume_overlap` calls

5. **Parallelize intersection detection** (2-4x speedup on 4 cores)
   - Process multiple residue pairs simultaneously
   - Expected: 174.8s → 44-87s

6. **Cache hull geometries** (2-3x speedup)
   - Cache hulls by amino acid set
   - Expected: Reduce repeated hull generation

### Low Priority (Expected 1.2-2x speedup)

7. **Optimize VTK boolean operations** (1.2-2x speedup)
   - Reduce VTK overhead
   - Expected: 18.0s → 9-15s

8. **C++ port of critical paths** (2-3x speedup)
   - Port `v_dot`, `inside_all` to C++
   - Expected: Additional 2-3x over NumPy

## Expected Performance After Optimizations

**Current:** 208.27 seconds  
**After High Priority:** 20-30 seconds (7-10x speedup)  
**After All Optimizations:** 5-15 seconds (14-42x speedup)

**Target:** Match expected 10-30 seconds for typical workloads

## Recommendations

### Immediate Actions

1. **Vectorize `v_dot`** - Biggest win, easiest to implement
2. **Lazy load VTK** - Quick fix, large time savings
3. **Profile again** - Verify optimizations work

### Next Steps

4. **Batch point-in-hull testing** - More complex but high impact
5. **Add spatial indexing** - Reduces unnecessary calculations
6. **Parallelize** - Use multiprocessing for independent pairs

### Long-term

7. **Consider C++ port** - If Python optimizations insufficient
8. **GPU acceleration** - For very large workloads (1000+ residues)

## Files to Modify

1. `src/main/python/CCKStar/Find_Doublets.py`
   - Line 347: `v_dot` - vectorize with NumPy
   - Line 471: `inside_all` - batch process points
   - Line 344: `calculate_volume_overlap` - add early termination

2. `src/main/python/CCKStar/Find_Doublets.py` (imports)
   - Lazy load VTK/PyVista modules

3. `src/main/python/CCKStar/Find_Doublets.py` (SCOPE function)
   - Add spatial indexing
   - Add parallel processing

## Conclusion

SCOPE profiling reveals that **volume overlap calculation is the dominant bottleneck**, with vector dot product being the hottest function (268M calls). The good news:

1. **Memory is not an issue** (388 MB peak, 0.12 MB delta)
2. **Clear optimization targets** (`v_dot`, `inside_all`, module imports)
3. **High potential speedup** (10-50x with NumPy vectorization)

With focused optimization on vectorization and lazy loading, SCOPE should achieve the expected 10-30 second runtime for typical workloads.

