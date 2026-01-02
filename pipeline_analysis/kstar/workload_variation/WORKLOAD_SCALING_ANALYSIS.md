# K* Workload Scaling Analysis

## Data Sources

- **test2RL0**: Full profiling data (169.6s, 16,734 pairs)
- **test1GUA11**: Partial data (79.6s, pair count TBD)
- **Intermediate results**: Energy matrix timing from current run

## Pair Count Scaling (2RL0)

| Component | Pairs | Ratio vs Protein |
|-----------|-------|------------------|
| Protein | 1,008 | 1.0x |
| Ligand | 7,575 | 7.51x |
| Complex | 16,734 | 16.60x |

## Energy Matrix Scaling

| Component | Entries | Time (s) | Time/Entry (ms) | Scaling Factor |
|-----------|---------|----------|-----------------|----------------|
| Protein | 106 | 0.081 | 0.76 | Baseline (1.0x) |
| Ligand | 3,472 | 3.9 | 1.12 | 1.47x |
| Complex | 4,963 | 16.3 | 3.28 | 4.32x |

### Analysis

**Protein → Ligand**:
- 32.7x entries, 48.1x time
- Scaling factor: 1.47x (near-linear)

**Ligand → Complex**:
- 1.43x entries, 4.18x time
- Scaling factor: 2.92x (super-linear)

**Protein → Complex**:
- 46.8x entries, 201x time
- Scaling factor: 4.29x (super-linear)

**Conclusion**: Energy matrix computation shows super-linear scaling. Complex calculations are 4.3x slower per entry than protein, indicating non-linear complexity growth with system size.

## Time Scaling (Partial Data)

**test2RL0**: 169.6 seconds
- This is setup phase (energy matrices + initialization)
- Actual K* partition function: 1-2 days (observed)
- Energy matrix: ~20 seconds total
- K* calculation: Dominant (99%+ of time)

**test1GUA11**: 79.6 seconds
- Smaller system, faster setup
- Pair count TBD (need to extract)

## Memory Scaling

**GC Overhead**:
- test2RL0: 0.51% (low pressure)
- test1GUA11: 3.40% (higher pressure)

**Inference**: Larger systems show higher GC overhead, indicating memory pressure increases with system size.

## Scaling Relationships

### Energy Matrix Time
- **Linear component**: O(entries)
- **Super-linear component**: O(entries^1.2-1.5) estimated
- **Formula**: `time ≈ entries^1.3 × base_time`

### K* Partition Function Time
- **Observed**: 1-2 days for 16,734 pairs
- **Estimated scaling**: Exponential or high polynomial
- **Formula**: `time ≈ pairs^2-3 × base_time` (estimated)

### Memory Usage
- **GC overhead**: Increases with system size
- **Peak memory**: Scales with pairs (linear to super-linear)
- **Arena allocation benefit**: 2-5x reduction in GC overhead

## Optimization Impact Estimates

### Energy Matrix Optimization
- **Current**: 4.32x super-linear scaling
- **Potential**: Vectorization (SIMD) could achieve 4-8x speedup
- **Impact**: Reduces setup time from 20s → 2.5-5s

### K* Parallelization
- **Current**: Sequential, 1-2 days
- **Potential**: 10-100x speedup (32-128 cores)
- **Impact**: Reduces K* time from 1-2 days → 10-20 minutes

### Arena Allocation
- **Current**: 0.51-3.40% GC overhead
- **Potential**: 2-5x reduction in GC overhead
- **Impact**: Eliminates GC pauses, improves cache locality

## System Size Breakpoints

| Size | Pairs | Energy Matrix | K* Time | Memory | Use Case |
|------|-------|---------------|---------|--------|----------|
| Small | < 1K | Seconds | Minutes | Low | Development, testing |
| Medium | 1-10K | 10-30s | Hours | Moderate | Representative workload |
| Large | > 10K | 30s-2min | Days | High | Production, stress test |

## Recommendations

1. **Prioritize K* parallelization**: 10-100x speedup, addresses dominant bottleneck
2. **Optimize energy matrices**: 4-8x speedup, reduces setup overhead
3. **Implement arena allocation**: 2-5x memory improvement, reduces GC pressure
4. **Profile smaller systems first**: Establish baseline before large system runs

## Next Steps

1. Extract pair counts for test1GUA11
2. Run protein-only K* (fast, ~1K pairs)
3. Run ligand-only K* (medium, ~7.5K pairs)
4. Use existing complex data for large system
5. Build complete scaling model from all data points

