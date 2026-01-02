# K* Workload Variation - Intermediate Results

## Current Status

**System**: Complex (Large)
**Expected pairs**: ~16,734
**Status**: K* calculation in progress

## Energy Matrix Calculation Times

### Protein Component
- **Entries**: 106
- **Time**: 80.6 ms
- **Time per entry**: 0.76 ms
- **Reference energies**: 14 position confs, 565.5 ms

### Ligand Component  
- **Entries**: 3,472
- **Time**: 3.9 s
- **Time per entry**: 1.12 ms
- **Reference energies**: 99 position confs, 75.8 ms

### Complex Component
- **Entries**: 4,963
- **Time**: 16.3 s
- **Time per entry**: 3.28 ms
- **Reference energies**: 113 position confs, 37.3 ms

## Observations

### Energy Matrix Scaling

| Component | Entries | Time (s) | Time/Entry (ms) | Scaling Factor |
|-----------|---------|----------|-----------------|----------------|
| Protein | 106 | 0.081 | 0.76 | Baseline |
| Ligand | 3,472 | 3.9 | 1.12 | 1.47x |
| Complex | 4,963 | 16.3 | 3.28 | 4.32x |

**Analysis**:
- Protein → Ligand: 32.7x entries, 48.1x time → **1.47x scaling factor** (near-linear)
- Ligand → Complex: 1.43x entries, 4.18x time → **2.92x scaling factor** (super-linear)
- Protein → Complex: 46.8x entries, 201x time → **4.29x scaling factor** (super-linear)

**Conclusion**: Energy matrix computation shows super-linear scaling as system size increases. Complex calculations are 4.3x slower per entry than protein.

### Reference Energy Calculation

| Component | Position Confs | Time (ms) | Time/Conf (ms) |
|-----------|----------------|-----------|----------------|
| Protein | 14 | 565.5 | 40.4 |
| Ligand | 99 | 75.8 | 0.77 |
| Complex | 113 | 37.3 | 0.33 |

**Analysis**:
- Protein reference energy calculation is anomalously slow (40.4 ms/conf vs. 0.33-0.77 ms/conf)
- Ligand and Complex show similar per-conf times
- May indicate initialization overhead or different computation complexity

## Expected K* Calculation

**Current**: Running K* for 1 sequence with epsilon = 0.99

**Based on previous profiling**:
- 2RL0 complex (16,734 pairs): ~1-2 days for full convergence
- This run (1 sequence): Expected 20-40 minutes for single sequence

## Next Steps

1. Wait for K* calculation to complete
2. Extract final timing and memory metrics
3. Compare with previous profiling data
4. Generate scaling relationships

