# Minimal Test Cases for Pipeline Analysis

This document identifies minimal but representative test cases for each OSPREY pipeline stage. These cases are designed to:
- Complete in minutes, not hours/days
- Exercise the same code paths as production workloads
- Generate meaningful profiling data
- Enable rapid iteration during development

## Test Case Selection Criteria

1. **Representative**: Uses same algorithms and data structures as production
2. **Minimal**: Smallest input that still exercises key code paths
3. **Fast**: Completes in < 10 minutes for profiling/iteration
4. **Reproducible**: Deterministic results for benchmarking

## Stage 1: SCOPE (Convex Hull Analysis)

### Minimal Test Case

**Test**: `test_scope_2rl0.py` (if exists) or minimal SCOPE call

**Input**:
- PDB: `examples/python.KStar/2RL0.min.reduce.pdb`
- Design chain: G (small subset of residues)
- Target chain: A
- Amino acids: 3-5 types (e.g., VAL, ALA, LEU) instead of all 22

**Expected Runtime**: 5-15 seconds

**Why Minimal**:
- 2RL0 is small structure (already minimal)
- Reducing amino acid types reduces hull generation time
- Still exercises all SCOPE algorithms (hull generation, intersection)

**Command**:
```bash
cd src/main/python/CCKStar
python3 -c "
from Find_Doublets import SCOPE
intrachain_pairs, interchain_pairs = SCOPE(
    '../examples/python.KStar/2RL0.min.reduce.pdb',
    'test_hulls',
    'G',
    ['VAL', 'ALA', 'LEU'],  # Minimal AA set
    True,
    'L',
    []
)
"
```

**Memory Profile**:
```bash
valgrind --tool=massif --massif-out-file=scope_minimal_massif.out \
    python3 -c "..."  # Above command
```

---

## Stage 2: MONTAGE (Scaffold Generation)

### Minimal Test Case

**Test**: Single MASTER match with minimal scaffold

**Input**:
- Single input PDB (from SCOPE output)
- MASTER matches: 1-2 matches (instead of 10-20)
- Max flexible residues: 2 (instead of 4)

**Expected Runtime**: 2-5 minutes (vs. 5-30 minutes for full)

**Why Minimal**:
- Single match eliminates parallel processing complexity
- Minimal flex reduces K* evaluation time
- Still exercises MASTER search, scaffold generation, K* prep

**Command**:
```python
from MONTAGE import run_MONTAGE
run_MONTAGE(
    "test_input",  # Single PDB directory
    "L",
    "L",
    1,  # Single match
    2   # Max 2 flexible residues
)
```

**Memory Profile**:
```bash
valgrind --tool=massif --massif-out-file=montage_minimal_massif.out \
    python3 montage_minimal.py
```

---

## Stage 3: Energy Matrix Computation

### Minimal Test Case

**Test**: `TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64`

**Input**:
- ConfSpace: 2RL0 (small structure)
- Conformations: 4-8 confs (minimal set)

**Expected Runtime**: 10-20 seconds

**Why Minimal**:
- Uses existing fast test infrastructure
- 2RL0 is standard small test case
- Still exercises all energy calculation code paths

**Command**:
```bash
./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" --no-daemon
```

**Memory Profile**:
```bash
valgrind --tool=massif --massif-out-file=energy_minimal_massif.out \
    ./gradlew test --tests "..." --no-daemon
```

---

## Stage 4: K* Algorithm (Partition Function)

### Minimal Test Case

**Option A: Single Sequence K* (Fastest)**

**Test**: Modified `test1GUA11()` with single sequence

**Input**:
- ConfSpace: 1GUA (smaller than 2RL0)
- Sequences: 1 sequence (instead of 25)
- Epsilon: 0.99 (less precise, faster convergence)

**Expected Runtime**: 1-3 minutes (vs. 10+ minutes for full test)

**Why Minimal**:
- Single sequence eliminates forced GC hack overhead
- Still exercises A* search, partition function, BigDecimal arithmetic
- 1GUA is smaller than 2RL0

**Command**:
```bash
# Create minimal test method in TestKStar.java
./gradlew test --tests "edu.duke.cs.osprey.kstar.TestKStar.test1GUAMinimal" --no-daemon
```

**Option B: MARKStar Tiny (Alternative)**

**Test**: `TestMARKStar.testMARKStarTinyEpsilon`

**Input**:
- numFlex: 3-5 (minimal flexibility)
- epsilon: 0.68 (faster convergence)

**Expected Runtime**: 2-5 minutes

**Command**:
```bash
./gradlew test --tests "edu.duke.cs.osprey.markstar.TestMARKStar.testMARKStarTinyEpsilon" --no-daemon
```

**Memory Profile**:
```bash
valgrind --tool=massif --massif-out-file=kstar_minimal_massif.out \
    ./gradlew test --tests "..." --no-daemon
```

**GC Log Analysis**:
```bash
export JAVA_OPTS="-Xmx2g -XX:+PrintGCDetails -XX:+PrintGCDateStamps -Xloggc:kstar_minimal_gc.log"
./gradlew test --tests "..." --no-daemon
```

---

## Stage 5: ARISE (Iterative Design)

### Minimal Test Case

**Test**: Single ARISE round with minimal sequences

**Input**:
- Starting structure: MONTAGE GMEC output
- Round 1 only (don't iterate)
- Sequences: 20-50 sequences (instead of 400)
- Single doublet (2 residues)

**Expected Runtime**: 5-10 minutes (vs. 1-3 hours for full)

**Why Minimal**:
- Single round eliminates iteration overhead
- Reduced sequence count (20-50 vs. 400) speeds up K* evaluation
- Still exercises ARISE graph construction, K* integration

**Command**:
```python
from ARISE import run_ARISE
run_ARISE(
    round_number=1,
    length_chain=4,  # Minimal chain length
    finished_matches=set(),
    visited_doublets={},
    design_id='B',
    target_id='A',
    final_designs_outfolder='test_arise',
    apo_tolerance=0.2,
    design_chirality='L'
)
# Modify ARISE to limit sequences to 20-50
```

---

## Complete Minimal Pipeline

### End-to-End Minimal Workflow

**Goal**: Run all stages with minimal inputs

**Runtime**: 10-20 minutes total (vs. hours for full pipeline)

**Stages**:
1. **SCOPE**: 2RL0 with 3 AA types (5-15s)
2. **MONTAGE**: 1 match, 2 flex residues (2-5 min)
3. **Energy Matrix**: 2RL0, 4-8 confs (10-20s)
4. **K***: Single sequence, 1GUA (1-3 min)
5. **ARISE**: Round 1, 20-50 sequences (5-10 min)

**Total**: ~10-20 minutes

**Command**:
```bash
# Run minimal pipeline
./scripts/profile_kstar_execution.sh --minimal
```

---

## Test Case Comparison

| Stage | Full Case | Minimal Case | Speedup | Representative? |
|-------|-----------|--------------|---------|-----------------|
| **SCOPE** | 2RL0, 22 AA types | 2RL0, 3-5 AA types | 5-10x | Yes (same algorithms) |
| **MONTAGE** | 10-20 matches, 4 flex | 1 match, 2 flex | 5-10x | Yes (same workflow) |
| **Energy Matrix** | 2RL0, many confs | 2RL0, 4-8 confs | 10-20x | Yes (same code paths) |
| **K* Algorithm** | 25 sequences, 2RL0 | 1 sequence, 1GUA | 5-10x | Yes (same A* search) |
| **ARISE** | Multiple rounds, 400 seqs | 1 round, 20-50 seqs | 10-20x | Yes (same iteration) |

---

## Creating Minimal Test Methods

### For K* Algorithm

Add to `TestKStar.java`:

```java
@Test
public void test1GUAMinimal() {
    // Single sequence, faster epsilon
    ConfSpaces confSpaces = make1GUA11();
    double epsilon = 0.99;  // Less precise, faster
    
    // Run only first sequence
    Result result = runKStar(confSpaces, epsilon, null, false, 1);
    
    // Check only first result
    assertSequence(result, 0, "HIE VAL", ...);
}
```

### For SCOPE

Create `test_scope_minimal.py`:

```python
from Find_Doublets import SCOPE

# Minimal AA set
minimal_aas = ['VAL', 'ALA', 'LEU']

intrachain_pairs, interchain_pairs = SCOPE(
    '2RL0.min.reduce.pdb',
    'test_hulls',
    'G',
    minimal_aas,  # Only 3 types
    True,
    'L',
    []
)
```

---

## Profiling Minimal Cases

### Memory Profiling

```bash
# SCOPE
valgrind --tool=massif --massif-out-file=scope_minimal_massif.out \
    python3 test_scope_minimal.py

# Energy Matrix
valgrind --tool=massif --massif-out-file=energy_minimal_massif.out \
    ./gradlew test --tests "TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64"

# K* Algorithm
valgrind --tool=massif --massif-out-file=kstar_minimal_massif.out \
    ./gradlew test --tests "TestKStar.test1GUAMinimal"
```

### GC Analysis

```bash
export JAVA_OPTS="-Xmx2g -XX:+PrintGCDetails -XX:+PrintGCDateStamps -Xloggc:gc.log -XX:+UseG1GC"
./gradlew test --tests "TestKStar.test1GUAMinimal" --no-daemon
```

### Perf Analysis

```bash
perf record -g --call-graph dwarf \
    ./gradlew test --tests "TestKStar.test1GUAMinimal" --no-daemon
perf report
```

---

## Expected Profiling Results

### Memory Patterns (from minimal cases)

**SCOPE**:
- Peak memory: 50-100 MB (vs. 200-500 MB for full)
- Allocation pattern: Same (hull generation, intersection)

**Energy Matrix**:
- Peak memory: 200-500 MB (vs. 1-5 GB for full)
- Allocation pattern: Same (energy matrix allocation)

**K* Algorithm**:
- Peak memory: 500 MB - 1 GB (vs. 1-10 GB for full)
- Allocation pattern: Same (A* tree, partition function)
- Forced GC: Still present (1 sequence = 1 forced GC)

---

## Integration with Analysis Tools

### Run Minimal Profiling

```bash
# Profile minimal K* case
./scripts/profile_kstar_execution.sh --minimal

# Analyze results
python3 scripts/analyze_pipeline_memory.py \
    --massif-file pipeline_analysis/memory/kstar_minimal_massif.out \
    --stage kstar \
    --output-dir pipeline_analysis/memory
```

### Generate Visualizations

```bash
# Create trace from minimal execution
python3 scripts/visualize_data_structures.py \
    --trace-file kstar_minimal_trace.csv \
    --output-dir visualizations
```

---


The minimal cases exercise the same code paths and data structures as production, just with smaller inputs. This makes them ideal for:
- Rapid iteration during development
- Memory profiling without waiting hours
- Understanding allocation patterns

