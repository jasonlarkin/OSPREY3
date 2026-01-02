# Test Case Problem Sizes

## Known Sizes (from code inspection)

### test2RL0
- **Complex pairs**: 16,734
- **Ligand pairs**: 7,575
- **Protein pairs**: 1,008
- **Complex singles**: 200
- **Ligand singles**: 143
- **Protein singles**: 57
- **Sequences**: 25 (from assert2RL0 assertions)
- **Duration**: 169.6s
- **GC Overhead**: 0.51%

### test1GUA11
- **PDB**: 1gua_adj.min.pdb
- **Protein residues**: 1-180 (180 residues)
- **Ligand residues**: 181-215 (35 residues)
- **Protein flexible positions**: 6 (21, 24, 25, 27, 29, 40)
- **Ligand flexible positions**: 2 (209, 213)
- **Sequences**: TBD (need to check assertions)
- **Duration**: 79.6s
- **GC Overhead**: 3.40%
- **Pair counts**: TBD (need to extract from test)

### test2RL0OnlyOneMutant
- **Base**: Same as test2RL0 (16,734 pairs complex)
- **Sequences**: 1 (single mutation)
- **Duration**: TBD

### test2RL0SpaceWithoutWildType
- **Base**: Same as test2RL0 (16,734 pairs complex)
- **Sequences**: 2 (from assertions)
- **Duration**: TBD

## Problem Size Classification

### By Conformation Space Size (Pairs)

| Category | Range | Test Cases |
|----------|-------|------------|
| **Very Small** | < 500 | None identified |
| **Small** | 500-2,000 | test2RL0 protein (1,008) |
| **Medium** | 2,000-10,000 | test2RL0 ligand (7,575) |
| **Large** | 10,000-20,000 | test2RL0 complex (16,734) |
| **Very Large** | > 20,000 | None identified |

### By Sequence Count

| Category | Range | Test Cases |
|----------|-------|------------|
| **Minimal** | 1 | test2RL0OnlyOneMutant |
| **Small** | 2-10 | test2RL0SpaceWithoutWildType (2) |
| **Medium** | 10-50 | test2RL0 (25), test1GUA11 (TBD) |
| **Large** | 50-100 | None identified |
| **Very Large** | > 100 | None identified |

## Coverage Gaps

1. **Very Small Problems** (< 500 pairs)
   - Need: Minimal test case for fast iteration
   - Could create: Single position, minimal rotamers

2. **Very Large Problems** (> 20,000 pairs)
   - Need: Stress test for production workloads
   - Could use: Larger PDB files from examples directory

3. **High Sequence Count** (> 50 sequences)
   - Need: Test with many sequences
   - Could use: Python examples or create synthetic

4. **test1GUA11 Characterization**
   - Need: Extract actual pair counts
   - Need: Extract sequence count
   - Action: Run test and extract from output or assertions

## Recommendations

1. **Extract test1GUA11 sizes**: Run test and check assertions or output
2. **Profile test2RL0OnlyOneMutant**: Smallest case (1 sequence)
3. **Profile test2RL0SpaceWithoutWildType**: Small sequence count (2 sequences)
4. **Check examples directory**: Look for larger systems
5. **Create minimal case**: If needed for very small problems

