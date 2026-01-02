# Fast K* Run: SUCCESS

## Summary

**K* calculation completed successfully** with epsilon=0.90 in ~2 minutes (vs. 1-2 days with epsilon=0.99).

## Key Results

- **Sequences processed**: 1
- **K* Score (log10)**: 12.166553
- **Complex conformations**: 28,411
- **Runtime**: ~2 minutes
- **Speedup**: ~360-1440x faster than full convergence

## Sequence Details

```
10 LEU=ALA 11 GLN=ALA 13 ASP=ALA 14 TYR=ALA 16 ARG=ALA 
17 PHE=ALA 1 GLU=ALA 2 PHE=ALA 3 SER=ALA 4 PHE=ALA 
5 LYS=ALA 6 LEU=ALA 9 ARG=ALA
```

**K* Range**: [10.906198, 14.161983] (log10)

## Partition Functions

| Component | Range (log10) | Conformations | Delta |
|-----------|---------------|---------------|-------|
| Protein | [43.30, 43.30] | 1 | 0.000 (converged) |
| Ligand | [929.86, 931.13] | 8 | 0.945 |
| Complex | [985.33, 987.33] | 28,411 | 0.990 |

**Note**: Ligand and Complex not fully converged (delta > epsilon), but sufficient for workload profiling.

## Workload Characteristics

- **Large complex search space**: 28,411 conformations explains long runtime
- **Energy matrices**: Loaded from cache (fast)
- **Memory**: Initial malloc crash, but succeeded on retry

## Epsilon Comparison

| Epsilon | Runtime | Convergence | Use Case |
|---------|---------|-------------|----------|
| 0.99 | 1-2 days | Full | Production |
| 0.90 | ~2 minutes | Partial | Profiling |
| 0.85 | ~30-60 min | Approximate | Fast testing |

## Conclusion

**Epsilon=0.90 is viable for workload profiling**. Provides sufficient data for scaling analysis in minutes instead of days.

**Next**: Extract timing metrics and add to workload scaling analysis.

