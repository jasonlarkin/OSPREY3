# OSPREY Tests to Port to C++

This file is a lightweight checklist of Java tests that motivated the C++ port.
It is intentionally high-level and should not duplicate the detailed status tracker.

To see what the C++ tree currently runs, use:

```bash
ctest --test-dir build/cpp/kstar -N
```

## A* Search Tests

### TestLinkedConfAStarNode.java (7 tests)
- `indexRoot` - Root node indexing
- `indexChild0` - Single assignment at position 0
- `indexChild3` - Single assignment at position 3
- `indexChild03` - Two assignments (0, 3)
- `indexChild30` - Two assignments (3, 0) - order independence
- `indexChild01234` - All positions assigned in order
- `indexChild43210` - All positions assigned in reverse order

### TestConfIndex.java (8 tests)
- Various conformation index tests

### TestConfRanker.java (5 tests)
- `tinyDiscrete1CC8` - 8 conformations, exhaustive check
- `small1CC8` - 2268 conformations, exhaustive check
- `medium1CC8` - 3.62e6 conformations, rank check (40306)
- `large1CC8` - 3.62e6 conformations, rank check (1034629)
- `huge1CC8` - 3.86e9 conformations, rank check (21039231)

### TestConfSearchCache.java (3 tests)
- `treeReinstantiation` - Cache tree re-instantiation
- `unrestrictedCapacity` - Unlimited cache capacity
- `restrictedCapacity` - Limited cache capacity (LRU)

### TestSMAStar.java (4 tests)
- SMA* search algorithm tests

### TestSequencePruner.java (1 test)
- Sequence pruning tests

## Partition Function Tests

### TestSimplePartitionFunction.java (40+ tests)

#### 2RL0 Protein (8 tests)
- Simple/GradientDescent × 1/2 CPUs × 1/4 GPU streams
- Expected Q*: "4.370068e+04" (epsilon=0.05)

#### 2RL0 Ligand (8 tests)
- Simple/GradientDescent × 1/2 CPUs × 1/4 GPU streams
- Expected Q*: "4.467797e+30" (epsilon=0.05)

#### 2RL0 Complex (10 tests)
- Simple/GradientDescent × 1/2/4 CPUs × 1/4 GPU streams
- Expected Q*: "3.5213742379e+54" (epsilon=0.8)

#### 1GUA11 Protein (2 tests)
- Simple/GradientDescent
- Expected Q*: "1.1838e+42" (epsilon=0.9)

#### 1GUA11 Ligand (2 tests)
- Simple/GradientDescent
- Expected Q*: "2.7098e+7" (epsilon=0.9)

#### 1GUA11 Complex (2 tests)
- Simple/GradientDescent
- Expected Q*: "1.1195e+66" (epsilon=0.9)

#### No Positions Protein (8 tests)
- Edge case: no flexible positions
- Expected Q*: "0.0e+0"

#### Other Tests
- `calcWithConfDB` - ConfDB integration
- `calcWithConfDBGD` - GradientDescent with ConfDB
- Various epsilon/parallelism combinations

## K* Tests

### TestKStar.java (6+ tests)
- `test2RL0` - Full K* calculation for 2RL0
- `test2RL0WithExternalMemory` - External memory mode
- `test1GUA11` - Full K* for 1GUA11
- `test2RL0WithConfDB` - ConfDB integration
- `test2RL0OnlyOneMutant` - Single mutation case
- `test2RL0SpaceWithoutWildType` - No wild-type sequence

### TestBBKStar.java (4+ tests)
- `test2RL0` - BBK* for 2RL0
- `test2RL0_MARKStar` - MARK* variant
- `test1GUA11` - BBK* for 1GUA11
- `test2RL0WithConfDB` - ConfDB integration

### TestMSKStar.java (4 tests)
- `test2RL0` - MSK* for 2RL0
- `test2RL0BoundedMemory` - Memory-bounded variant
- `test2RL0OnlyOneMutant` - Single mutation
- `test2RL0SpaceWithoutWildType` - No wild-type

### TestKStarScore.java (30+ tests)
- Various K* score calculation scenarios
- Different bound combinations (P/L/C, p/l/c)
- Edge cases and boundary conditions

### TestSequenceAnalyzer.java (1 test)
- `test2RL0` - Sequence analysis

## Compiled ConfSpace Tests

### compiled/TestKStar.java
- `test2RL0_DesmetEtAl1992` - K* with compiled conf spaces

### compiled/TestBounds.java
- `test2RL0Complex` - Bounds calculation

### compiled/TestBBKStar.java
- `test2RL0` - BBK* with compiled conf spaces

## Priority Order for Porting

1. **Phase‑1 (EnergyMatrix-only)**:
   - PartitionFunction contracts + A* correctness/equivalence
   - ConfIndex / ConfRanker / ConfSearchCache / KStarScore logic ports
   - Synthetic precision tiers (exact enum oracle on tiny spaces)

2. **Phase‑2+ (requires ConfSpace + real energy pipeline)**:
   - Full K* tests (`TestKStar`, `TestBBKStar`, `TestMSKStar`) and ConfDB integration tests

## Test Data Requirements

- PDB files: `/2RL0.min.reduce.pdb`, `/1CC8.ss.pdb`, `/1GUA11.pdb`
- Energy matrices: exported from Java in a C++-readable binary format for parity tests; otherwise synthetic
- Expected Q* values: From OSPREY test assertions
- ConfSpace definitions: From test setup code

