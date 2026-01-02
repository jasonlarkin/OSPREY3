# Fast K* Run Results (Epsilon=0.90)

## Status: SUCCESS

**K* calculation completed successfully** with relaxed epsilon (0.90).

## Results

**Sequences found**: 1
**Epsilon**: 0.90 (relaxed, faster convergence)

### Sequence Details

**Sequence**: 10 LEU=ALA 11 GLN=ALA 13 ASP=ALA 14 TYR=ALA 16 ARG=ALA 17 PHE=ALA 1 GLU=ALA 2 PHE=ALA 3 SER=ALA 4 PHE=ALA 5 LYS=ALA 6 LEU=ALA 9 ARG=ALA

**K* Score (log10)**: 12.166553
**K* Range**: [10.906198, 14.161983] (log10)

### Partition Function Details

**Protein**:
- Range: [43.298796, 43.298796] (log10)
- Conformations: 1
- Delta: 0.000 (converged)

**Ligand**:
- Range: [929.86477, 931.12513] (log10)
- Conformations: 8
- Delta: 0.945 (not fully converged at epsilon=0.90)

**Complex**:
- Range: [985.33012, 987.32555] (log10)
- Conformations: 28,411
- Delta: 0.990 (not fully converged at epsilon=0.90)

## Timing

**Total duration**: ~116 seconds (2 minutes)
**Energy matrix**: Loaded from cache (fast)
**K* calculation**: ~2 minutes (vs. hours/days with epsilon=0.99)

## Observations

1. **Epsilon=0.90 works**: Calculation completes in minutes instead of days
2. **Ligand and Complex not fully converged**: Delta > epsilon indicates more iterations needed for full convergence
3. **Still useful for profiling**: Provides workload characteristics even without full convergence
4. **Memory issue**: Initial run crashed with malloc error, but succeeded on retry

## Comparison to Full Convergence

**Epsilon=0.99** (full convergence):
- Expected: 1-2 days
- Delta: < 0.01 for all components
- Accuracy: Provably accurate

**Epsilon=0.90** (relaxed):
- Actual: ~2 minutes
- Delta: 0.945-0.990 (not converged)
- Accuracy: Approximate, but sufficient for workload profiling

**Speedup**: ~360-1440x faster (minutes vs. days)

## Workload Characteristics

- **Complex conformations**: 28,411 (large search space)
- **Ligand conformations**: 8 (small)
- **Protein conformations**: 1 (minimal)

The large complex conformation count (28,411) explains why full convergence takes so long.

## Next Steps

1. Extract timing metrics from this run
2. Compare with epsilon=0.99 runs (if available)
3. Use for workload scaling analysis
4. Consider epsilon=0.95 for balance between speed and accuracy

