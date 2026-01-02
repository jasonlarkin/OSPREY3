# K* Algorithm Performance Optimization Analysis

## Overview

The K* algorithm is a key component of OSPREY that predicts protein sequence mutations that improve binding affinity by computing provably accurate Boltzmann-weighted ensembles. This document analyzes the current implementation and identifies performance optimization opportunities.

## Algorithm Structure

### Core Components

1. **KStar.java** (`src/main/java/edu/duke/cs/osprey/kstar/KStar.java`)
   - Main K* implementation
   - Processes sequences serially
   - Computes partition functions for protein, ligand, and complex states

2. **KStarDirector.java** (`src/main/java/edu/duke/cs/osprey/coffee/directors/KStarDirector.java`)
   - COFFEE-based K* implementation
   - Uses MultiStateConfSpace
   - Also processes sequences serially

3. **PartitionFunction** (`src/main/java/edu/duke/cs/osprey/kstar/pfunc/PartitionFunction.java`)
   - Core partition function calculation
   - Uses A* search tree traversal
   - Performs Boltzmann weighting with BigDecimal arithmetic

### K* Score Calculation

The K* score is calculated as:
```
K* = Q_complex / (Q_protein × Q_ligand)
```

Where Q represents the partition function for each state.

## Current Performance Bottlenecks

### 1. Serial Sequence Processing (High Impact)

**Location**: `KStar.java:562-585`

```java
// compute all the partition functions and K* scores for the rest of the sequences
for (int i=1; i<n; i++) {
    Sequence seq = sequences.get(i);
    
    // get the pfuncs, with short circuits as needed
    proteinResult = protein.calcPfunc(ctxGroup, seq, proteinStabilityThreshold);
    ligandResult = ligand.calcPfunc(ctxGroup, seq, ligandStabilityThreshold);
    complexResult = complex.calcPfunc(ctxGroup, seq, BigDecimal.ZERO);
    
    scorer.score(i, proteinResult, ligandResult, complexResult);
}
```

**Problem**: Sequences are processed one at a time, even though partition function calculations for different sequences are independent.

**Impact**: For N sequences, total time = N × (time_per_sequence). With parallelization, could approach time_per_sequence.

**Optimization Opportunity**: Parallelize sequence processing using Java's `ExecutorService` or `CompletableFuture`.

### 2. Forced Garbage Collection Hack (Medium Impact)

**Location**: `KStar.java:374-382`

```java
/* HACKHACK: we're done using the A* tree, pfunc, etc
    and normally the garbage collector will clean them up,
    along with their off-heap resources (e.g. TPIE data structures).
    Except the garbage collector might not do it right away.
    If we try to allocate more off-heap resources before these get cleaned up,
    we might run out. So poke the garbage collector now and try to get
    it to clean up the off-heap resources right away.
*/
Runtime.getRuntime().gc();

// newer JVMs have more concurrent garbage collectors
// give it a little time to finish cleaning up the pfunc
try {
    Thread.sleep(10);
} catch (InterruptedException ex) {
    throw new RuntimeException(ex);
}
```

**Problem**: 
- Forces a full GC after each partition function calculation
- Adds 10ms sleep per sequence
- For 100 sequences, this adds 1 second of pure overhead

**Impact**: 
- GC pauses can be expensive (10-100ms)
- Sleep adds fixed overhead
- May not be necessary with modern JVMs and better memory management

**Optimization Opportunity**: 
- Investigate if this is still necessary with modern JVMs
- Consider explicit resource cleanup instead of relying on GC
- Use try-with-resources or explicit close() methods for off-heap resources

### 3. Partition Function Computation (Very High Impact)

**Location**: `KStar.java:354-360`, `PartitionFunction.compute()`

The partition function computation involves:
1. A* search tree traversal
2. Energy calculations (calls to C++ via JNA)
3. Boltzmann weighting with BigDecimal arithmetic
4. Conformation enumeration

**Problem**: This is the core computational bottleneck. Each partition function can take seconds to minutes depending on:
- Size of conformation space
- Epsilon precision requirement
- Energy calculation performance

**Impact**: This dominates total runtime. Even with other optimizations, this remains the bottleneck.

**Optimization Opportunities**:
- **Already optimized**: Energy calculations are now in optimized C++ code
- **Early termination**: Already implemented with stability thresholds
- **Caching**: Already implemented per ConfSpaceInfo
- **Potential**: Better A* search heuristics, incremental computation

### 4. BigDecimal Arithmetic Overhead (Medium Impact)

**Location**: Throughout `PartitionFunction` and `KStarScore`

**Problem**: BigDecimal arithmetic is significantly slower than double arithmetic, but necessary for provable accuracy.

**Impact**: 
- BigDecimal operations are 10-100x slower than double
- Used extensively in partition function calculations
- Required for provable accuracy guarantees

**Optimization Opportunity**: 
- Profile to identify hot spots
- Consider using double for intermediate calculations where precision isn't critical
- Use BigDecimal only for final results and bounds

### 5. Memory Allocation Patterns (Low-Medium Impact)

**Location**: Throughout K* code

**Problem**: 
- New PartitionFunction objects created for each sequence
- A* search trees allocated and deallocated frequently
- Off-heap memory (TPIE) may not be released promptly

**Impact**: 
- GC pressure
- Memory fragmentation
- Potential out-of-memory errors for large designs

**Optimization Opportunity**:
- Object pooling for PartitionFunction instances
- Reuse A* search trees where possible
- Better off-heap memory management

## Optimization Recommendations

### Priority 1: Parallelize Sequence Processing

**Effort**: Medium
**Impact**: High
**Risk**: Low-Medium (need to ensure thread safety)

**Implementation**:
1. Use `ExecutorService` with thread pool
2. Process sequences in parallel batches
3. Ensure thread-safe access to shared resources (caches, score writers)
4. Maintain determinism for reproducibility

**Example Structure**:
```java
ExecutorService executor = Executors.newFixedThreadPool(numThreads);
List<Future<ScoredSequence>> futures = new ArrayList<>();

for (int i=1; i<n; i++) {
    final int seqIndex = i;
    futures.add(executor.submit(() -> {
        // Compute partition functions for this sequence
        // Return ScoredSequence
    }));
}

// Collect results
for (Future<ScoredSequence> future : futures) {
    ScoredSequence result = future.get();
    // Process result
}
```

### Priority 2: Remove or Optimize GC Hack

**Effort**: Low
**Impact**: Medium
**Risk**: Low (can be tested incrementally)

**Implementation**:
1. Profile memory usage to determine if GC hack is still needed
2. If needed, replace with explicit resource cleanup
3. Remove Thread.sleep() if not necessary
4. Consider using modern JVM flags for better GC behavior

### Priority 3: Optimize Partition Function Computation

**Effort**: High
**Impact**: Very High
**Risk**: Medium (core algorithm, must maintain correctness)

**Implementation**:
1. Profile partition function computation to identify hot spots
2. Optimize A* search heuristics
3. Consider incremental computation for similar sequences
4. Cache intermediate results more aggressively

### Priority 4: Profile and Optimize BigDecimal Usage

**Effort**: Medium
**Impact**: Medium
**Risk**: Low (can be done incrementally)

**Implementation**:
1. Profile to find BigDecimal hot spots
2. Use double for intermediate calculations where precision allows
3. Convert to BigDecimal only for final results and bounds

## Performance Benchmarking

### Current Baseline

To establish a baseline, run K* on a standard test case:
- **Test case**: 2RL0 example
- **Sequences**: Vary from 10 to 100+
- **Metrics**: 
  - Total runtime
  - Time per sequence
  - Partition function computation time
  - Memory usage
  - GC overhead

### Benchmarking Script

Create a script similar to `benchmark_performance.sh` but for K*:
- Run K* with different numbers of sequences
- Measure time per sequence
- Profile memory usage
- Compare before/after optimizations

## Implementation Plan

1. **Phase 1: Profiling** (1-2 days)
   - Add detailed timing to K* code
   - Profile memory usage
   - Identify hot spots

2. **Phase 2: Low-Hanging Fruit** (2-3 days)
   - Remove/optimize GC hack
   - Profile BigDecimal usage
   - Optimize memory allocation patterns

3. **Phase 3: Parallelization** (3-5 days)
   - Implement parallel sequence processing
   - Add thread safety
   - Test and benchmark

4. **Phase 4: Advanced Optimizations** (5-10 days)
   - Optimize partition function computation
   - Improve A* search heuristics
   - Incremental computation for similar sequences

## Related Files

- `src/main/java/edu/duke/cs/osprey/kstar/KStar.java` - Main K* implementation
- `src/main/java/edu/duke/cs/osprey/kstar/pfunc/PartitionFunction.java` - Partition function interface
- `src/main/java/edu/duke/cs/osprey/kstar/pfunc/ParallelConfPartitionFunction.java` - Parallel partition function
- `src/main/java/edu/duke/cs/osprey/coffee/directors/KStarDirector.java` - COFFEE-based K*
- `src/test/java/edu/duke/cs/osprey/kstar/TestKStar.java` - K* tests
- `examples/python.KStar/kstar.py` - Python K* example

## References

- Lilien et al. (2005). "A Novel Ensemble-Based Scoring and Search Algorithm for Protein Redesign"
- OSPREY documentation on K* algorithm
- Performance benchmarking guide: `docs/performance/PERFORMANCE_BENCHMARKING_GUIDE.md`

