# Workload Variation Analysis: From System Size to Pipeline Stages

## Overview

This document analyzes how molecular system size (atoms, residues, pairs) propagates through each OSPREY pipeline stage, affecting computational workload.

## System Size Dimensions

### Input Dimensions
1. **Number of atoms** - Total atoms in structure
2. **Number of residues** - Protein/ligand chain length
3. **Number of chains** - Multi-chain complexes
4. **Conformation space size** - Rotamer pairs (Amber + EEF1)
5. **Sequence space size** - Number of mutations/sequences to evaluate

### Example Systems

| System | Atoms | Residues | Complex Pairs | Ligand Pairs | Protein Pairs | Sequences |
|--------|-------|----------|---------------|--------------|--------------|-----------|
| **2RL0** | ~2000 | ~200 | 16,734 | 7,575 | 1,008 | Multiple |
| **1GUA11** | TBD | TBD | TBD | TBD | TBD | Multiple |
| **Small test** | 200 | ~20 | 3,000 | 2,000 | 1,000 | 1-5 |

## Workload Propagation Through Pipeline

### Stage 1: SCOPE (Convex Hull Analysis)

**Input:** PDB structure
**Output:** Convex hulls for each residue pair

**Workload Scaling:**
- **O(n²)** where n = number of residues
- Each residue pair requires:
  - Convex hull computation (geometric)
  - Volume overlap calculation (VTK operations)
  - Intersection testing

**Size Impact:**
- Small (20 residues): ~200 pairs → seconds
- Medium (100 residues): ~5,000 pairs → minutes
- Large (200 residues): ~20,000 pairs → hours

**Bottlenecks:**
- VTK boolean operations (volume overlap)
- File I/O (hull storage)

**Variation Factors:**
- **Residue flexibility** - More flexible residues = more complex hulls
- **Chain count** - Multi-chain = more inter-chain pairs
- **Structure complexity** - Compact vs. extended conformations

### Stage 2: MONTAGE (Scaffold Generation)

**Input:** SCOPE hulls + target structure
**Output:** Scaffold matches (PDB files)

**Workload Scaling:**
- **O(m × n)** where:
  - m = number of MASTER queries (depends on flexibility)
  - n = database size
- Each query:
  - MASTER C++ subprocess: ~12 seconds per match (observed: 63s for 5 matches)
  - ConfSpace compilation: O(pairs) - scales with conformation space, **DOMINANT** (80% of time)
  - File I/O: Match PDBs, confspace files

**Size Impact:**
- Small: 1-5 queries → ~30-60 minutes (dominated by ConfSpace compilation)
- Medium: 5-10 queries → ~1-2 hours (dominated by ConfSpace compilation)
- Large: 10-20 queries → ~2-4 hours (dominated by ConfSpace compilation)

**Bottlenecks:**
- **ConfSpace compilation** - **DOMINANT** (80% of time, 1744s/2191s observed), Java, scales with pairs
- **SCOPE operations** - 8.6% of time (188s), volume overlap calculations
- **MASTER subprocess** - Fast (~12s/match), not a bottleneck
- **File I/O** - Match PDBs, confspace files

**Variation Factors:**
- **Flexibility settings** - More flexibility = more queries
- **Target complexity** - Larger targets = longer MASTER searches
- **Match count** - More matches = more compilation work

### Stage 3: K* Execution (Partition Function)

**Input:** Compiled confspaces (.ccsx files)
**Output:** K* scores per sequence

**Workload Scaling:**
- **O(sequences × pairs × convergence_iterations)**
- Each sequence:
  - A* search tree construction
  - Energy calculations (C++ native)
  - Partition function computation
  - Convergence checking (delta <= epsilon)

**Size Impact:**
- Small (1,000 pairs, 1 sequence): ~seconds
- Medium (10,000 pairs, 10 sequences): ~minutes
- Large (16,734 pairs, 100 sequences): **1-2 days** (observed)

**Bottlenecks:**
- **Energy calculations** - C++ native code (JNI overhead visible)
- **A* search** - Memory-intensive tree growth
- **Partition function** - Computational complexity
- **Memory allocation** - Per-sequence churn (GC overhead)

**Variation Factors:**
- **Conformation space size** - More pairs = exponential search space
- **Sequence count** - Linear scaling per sequence
- **Convergence epsilon** - Tighter = more iterations
- **Energy landscape** - Smooth vs. rugged affects A* efficiency

### Stage 4: ARISE (Iterative Design)

**Input:** K* results (submit.out files)
**Output:** Final designed sequences

**Workload Scaling:**
- **O(matches × iterations × K*_time)**
- Each iteration:
  - Read K* results (I/O)
  - Find best doublets (computation)
  - Update graph (SCOPE re-run)
  - Generate new sequences

**Size Impact:**
- Small (1 match, 5 iterations): ~minutes
- Medium (10 matches, 10 iterations): ~hours
- Large (100 matches, 20 iterations): **days to weeks**

**Bottlenecks:**
- **K* dependency** - Must wait for K* convergence
- **SCOPE re-runs** - Each iteration re-computes graph
- **File I/O** - Reading K* results, writing sequences

**Variation Factors:**
- **Number of matches** - Linear scaling
- **Iteration count** - Depends on chain length
- **K* convergence time** - Dominant factor (propagates from Stage 3)

## Workload Variation Summary

### Propagation Chain

```
System Size (atoms/residues)
    ↓
SCOPE: O(n²) pairs → hull computation time
    ↓
MONTAGE: O(queries) → ConfSpace compilation (80%) + MASTER time (fast) + SCOPE
    ↓
K*: O(sequences × pairs × iterations) → **DOMINANT BOTTLENECK**
    ↓
ARISE: O(matches × iterations × K*_time) → **BLOCKED BY K***
```

### Key Observations

1. **K* is the dominant bottleneck** - 1-2 days for medium systems
2. **Workload is multiplicative** - Each stage amplifies previous stage's size
3. **Memory scales with pairs** - Larger systems = more memory pressure
4. **File I/O is significant** - Multi-process architecture creates I/O overhead

### Size-to-Workload Mapping

| System Size | SCOPE | MONTAGE | K* | ARISE | Total |
|-------------|-------|---------|----|----|-------|
| **Small** (20 res, 1K pairs) | Seconds | Minutes | Minutes | Minutes | ~1 hour |
| **Medium** (100 res, 10K pairs) | Minutes | Hours | Hours | Hours | ~1 day |
| **Large** (200 res, 17K pairs) | Hours | Hours | **1-2 days** | Days | **Weeks** |

## Implications for Multiprocessing

### Current Architecture Issues

1. **Sequential stages** - Each stage waits for previous
2. **File I/O bottlenecks** - Multi-process communication via files
3. **Memory fragmentation** - JVM heap + native memory
4. **No shared memory** - Data copied between processes

### Unified C++ Architecture Benefits

1. **Shared memory** - Zero-copy data structures
2. **Parallel stages** - Can pipeline stages
3. **Unified memory** - Single memory space, arena allocation
4. **MPI parallelism** - Can parallelize K* across nodes

### Workload-Specific Optimizations

**Small systems:**
- Overhead dominates → focus on reducing startup cost
- Single-threaded may be sufficient

**Medium systems:**
- K* dominates → parallelize K* sequences
- Memory management critical

**Large systems:**
- K* is bottleneck → MPI + shared memory
- Need cluster-scale parallelism
- Arena allocation essential

## Next Steps

1. Run profiling on multiple system sizes
2. Measure K* time vs. pair count
3. Analyze memory usage vs. system size
4. Document workload variation in each stage
5. Design unified architecture to handle full range

