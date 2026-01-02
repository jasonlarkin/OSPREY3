# Problem Size Coverage Analysis

## Current Test Cases

### Profiled Test Cases

| Test Case | Complex Pairs | Ligand Pairs | Protein Pairs | Total Singles | Duration | GC Overhead |
|-----------|---------------|--------------|---------------|---------------|----------|-------------|
| **test2RL0** | 16,734 | 7,575 | 1,008 | 200 | 169.6s | 0.51% |
| **test1GUA11** | TBD | TBD | TBD | TBD | 79.6s | 3.40% |

### Available Test Cases (Not Yet Profiled)

| Test Case | Description | Expected Size |
|-----------|-------------|---------------|
| **test2RL0OnlyOneMutant** | 2RL0 with single mutation | Smaller (fewer sequences) |
| **test2RL0SpaceWithoutWildType** | 2RL0 without wild type | Similar pairs, fewer sequences |
| **test2RL0WithExternalMemory** | 2RL0 with TPIE | Same size, different memory model |
| **test2RL0WithConfDB** | 2RL0 with conformation DB | Same size, different storage |

## Problem Size Dimensions

### 1. Conformation Space Size (Pairs)

**Current Coverage:**
- Small: 1,008 pairs (test2RL0 protein)
- Medium: 7,575 pairs (test2RL0 ligand)
- Large: 16,734 pairs (test2RL0 complex)

**Gaps:**
- Very small: < 500 pairs (for testing minimal cases)
- Very large: > 20,000 pairs (for stress testing)

### 2. Sequence Space Size

**Current Coverage:**
- test2RL0: Multiple sequences (exact count TBD)
- test1GUA11: Multiple sequences (exact count TBD)
- test2RL0OnlyOneMutant: 1 sequence (minimal)

**Gaps:**
- Need to identify exact sequence counts
- Need very large sequence spaces (> 100 sequences)

### 3. System Complexity

**Current Coverage:**
- test2RL0: Medium complexity (protein + ligand)
- test1GUA11: Different system (need to characterize)

**Gaps:**
- Simple systems (protein only or ligand only)
- Very complex systems (multiple chains, large proteins)

## Recommendations

### Minimum Coverage Needed

1. **Small Problem** (< 1,000 pairs, 1-5 sequences)
   - Purpose: Fast iteration, minimal test cases
   - Candidate: test2RL0OnlyOneMutant (if it's small enough)
   - Gap: May need to create smaller test case

2. **Medium Problem** (1,000-10,000 pairs, 10-50 sequences)
   - Purpose: Representative workload
   - Current: test2RL0 (16,734 pairs - actually large)
   - Gap: Need true medium-sized case (5,000-8,000 pairs)

3. **Large Problem** (> 10,000 pairs, 50+ sequences)
   - Purpose: Stress testing, production-like
   - Current: test2RL0 complex (16,734 pairs)
   - Gap: Need even larger cases (> 20,000 pairs)

### Action Items

1. **Characterize test1GUA11**
   - Extract pair counts and sequence counts
   - Determine where it fits in problem size space

2. **Characterize variants**
   - test2RL0OnlyOneMutant: Sequence count, pair counts
   - test2RL0SpaceWithoutWildType: Sequence count, pair counts

3. **Identify gaps**
   - Very small: < 500 pairs
   - Very large: > 20,000 pairs
   - High sequence count: > 100 sequences

4. **Find or create missing test cases**
   - Check examples directory for larger systems
   - Check if Python examples have different sizes
   - Consider creating synthetic test cases

## Next Steps

1. Extract problem sizes from all available test cases
2. Map test cases to problem size dimensions
3. Identify gaps in coverage
4. Prioritize which test cases to profile next
5. Create additional test cases if gaps significant

