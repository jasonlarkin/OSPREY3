# CCKStar (Python) vs Java KStar Comparison

## Overview

CCKStar is a Python workflow/orchestration layer that uses geometric analysis (convex hulls) to guide protein design, then calls the Java K* algorithm as a subprocess. The Java K* algorithm performs the actual partition function calculations.

## Architecture

### Java KStar (`src/main/java/edu/duke/cs/osprey/kstar/KStar.java`)

**Purpose**: Core K* algorithm implementation
- Computes partition functions for protein, ligand, and complex states
- Calculates K* scores: `K* = Q_complex / (Q_protein × Q_ligand)`
- Processes sequences and evaluates binding affinity
- Pure computational algorithm

**Key Features**:
- Partition function calculation using A* search
- Boltzmann weighting with BigDecimal arithmetic
- Sequence processing (serial)
- Energy calculations via JNA to optimized C++ code
- Provable accuracy guarantees (epsilon-approximation)

**Usage**:
- Direct Java API: `KStar kstar = new KStar(...); kstar.run()`
- Python API: `osprey.KStar(...).run()`
- Command line: `osprey3 kstar --complex-confspace ... --target-confspace ... --design-confspace ...`

### CCKStar (Python) (`src/main/python/CCKStar/`)

**Purpose**: Workflow orchestration for de novo peptide design
- Uses geometric analysis to guide design decisions
- Prepares OSPREY conformation spaces programmatically
- Calls Java K* as subprocess
- Iterative sequence design workflow

**Key Components**:

1. **SCOPE** (`Find_Doublets.py`)
   - Side Chain Orientation and Position Evaluation
   - Uses convex hulls to determine side chain contacts
   - Identifies "doublets" (intra-chain intersections) and "islands" (residues with no intra-chain intersections)
   - Prioritizes mutations and flexibility based on geometric overlap

2. **MONTAGE** (`MONTAGE.py`)
   - Motif-Oriented Noncanonical Template Assembly and Generation Engine
   - Generates L or D polyalanine scaffolds for L targets
   - Uses MASTER database for motif matching
   - Runs K* to rank scaffolds by designability

3. **ARISE** (`ARISE.py`)
   - Affinity-driven Rational Iteration for Sequence Engineering
   - Iterative sequence design algorithm
   - Uses SCOPE to build contact graph
   - Mutates 2 connected residues at a time (400 sequences per iteration)
   - Recomputes contact graph after each iteration
   - Continues until full sequence is designed

4. **KStarPrep** (`KStarPrep.py`)
   - Prepares OSPREY files for K* runs
   - Creates conformation spaces using `osprey.prep` Python API
   - Compiles to `.ccsx` files
   - Organizes files for batch K* execution

## Key Differences

### 1. **Geometric Analysis**

**CCKStar**: 
- Uses convex hull analysis to determine which residues to mutate
- Uses volume overlap to prioritize flexible residues
- Reduces search space based on geometric constraints

**Java KStar**:
- No geometric analysis
- Processes all sequences in the provided conformation space
- Relies on user to specify which residues to mutate

### 2. **Workflow Orchestration**

**CCKStar**:
- Multi-step iterative workflow
- Automates sequence selection based on previous results
- Manages file organization and batch processing
- Handles cluster job submission (SLURM)

**Java KStar**:
- Single-shot algorithm
- Processes provided sequences
- No workflow automation

### 3. **Invocation Method**

**CCKStar**:
```python
# Prepares files, then calls:
subprocess.run(["./resources/K_bash.sh"])  # Which runs:
# osprey3 kstar --complex-confspace ./complex.ccsx ...
```

**Java KStar**:
```java
// Direct API call
KStar kstar = new KStar(protein, ligand, complex, settings);
List<ScoredSequence> results = kstar.run();
```

### 4. **Sequence Selection**

**CCKStar**:
- Uses SCOPE to identify promising mutation sites
- ARISE iteratively builds sequences 2 residues at a time
- Selects next mutations based on contact graph

**Java KStar**:
- Processes all sequences in conformation space
- No intelligent sequence selection
- User must specify mutation space

### 5. **Flexibility Determination**

**CCKStar**:
- Uses convex hull intersections to determine which target residues should be flexible
- Ranks flexibility by volume overlap
- Reduces flexibility to `max_flex` residues based on overlap

**Java KStar**:
- User must specify flexible residues
- No automatic flexibility determination

## Performance Implications

### CCKStar Advantages

1. **Reduced Search Space**: Geometric analysis reduces the number of sequences to evaluate
2. **Intelligent Prioritization**: Focuses on geometrically promising mutations
3. **Iterative Refinement**: ARISE builds sequences incrementally, avoiding full enumeration

### CCKStar Overhead

1. **Convex Hull Computation**: Time spent computing hulls and intersections
2. **File I/O**: Multiple file operations for organizing K* runs
3. **Subprocess Overhead**: Calling Java K* via command line adds startup overhead
4. **Python API Overhead**: Using `osprey.prep` Python API instead of direct Java calls

### Java KStar Advantages

1. **Direct API**: No subprocess overhead
2. **In-Memory Processing**: Can process sequences without file I/O
3. **Better Integration**: Can be called directly from Java code

### Java KStar Limitations

1. **No Geometric Guidance**: Must evaluate all sequences in space
2. **No Workflow Automation**: Manual sequence selection required
3. **Serial Processing**: Processes sequences one at a time (optimization opportunity)

## Code Flow Comparison

### CCKStar Workflow (ARISE example)

```
1. SCOPE analysis → Identify doublets and islands
2. Choose next doublet based on contact graph
3. Reduce flexibility based on volume overlap
4. KStarPrep → Create conformation spaces
5. Compile to .ccsx files
6. Call Java K* via subprocess (K_bash.sh)
7. Parse results from log files
8. Select best sequence
9. Update contact graph
10. Repeat until full sequence designed
```

### Java KStar Workflow

```
1. User provides conformation spaces
2. Collect all sequences to evaluate
3. For each sequence:
   a. Compute protein partition function
   b. Compute ligand partition function
   c. Compute complex partition function
   d. Calculate K* score
4. Return scored sequences
```

## Optimization Opportunities

### For CCKStar

1. **Parallel K* Calls**: Currently calls K* sequentially; could parallelize
2. **Caching**: Cache convex hull computations
3. **Direct API**: Use Java K* API directly instead of subprocess (via JPype)
4. **Batch Processing**: Group multiple K* runs into single batch

### For Java KStar

1. **Parallel Sequence Processing**: Process sequences in parallel (see `KSTAR_PERFORMANCE_ANALYSIS.md`)
2. **Geometric Pruning**: Add optional geometric analysis to prune sequences early
3. **Incremental Computation**: Reuse partition function computations for similar sequences

## When to Use Which

### Use Java KStar When:
- You have a well-defined sequence space
- You want direct control over which sequences to evaluate
- You're integrating into Java/Kotlin code
- You need maximum performance (no subprocess overhead)
- You're doing single-shot designs

### Use CCKStar When:
- You're doing de novo design (starting from scratch)
- You need geometric guidance for mutation selection
- You want iterative sequence building (ARISE)
- You're designing L/D peptides for L targets
- You need scaffold generation (MONTAGE)
- You want automated workflow management

## Files Reference

### CCKStar Files
- `src/main/python/CCKStar/Find_Doublets.py` - SCOPE algorithm
- `src/main/python/CCKStar/MONTAGE.py` - Scaffold generation
- `src/main/python/CCKStar/ARISE.py` - Iterative sequence design
- `src/main/python/CCKStar/KStarPrep.py` - OSPREY file preparation
- `src/main/python/CCKStar/resources/K_bash.sh` - K* invocation script

### Java KStar Files
- `src/main/java/edu/duke/cs/osprey/kstar/KStar.java` - Main K* implementation
- `src/main/java/edu/duke/cs/osprey/kstar/pfunc/PartitionFunction.java` - Partition function interface
- `src/main/java/edu/duke/cs/osprey/coffee/directors/KStarDirector.java` - COFFEE-based K*

## Summary

CCKStar is **not** a replacement for Java K*, but rather a **workflow layer** that:
1. Uses geometric analysis to guide design decisions
2. Automates iterative sequence design
3. Manages file preparation and batch processing
4. Calls Java K* as the computational engine

The actual K* calculations are still performed by the Java code, which benefits from all the C++ optimizations we've implemented. CCKStar adds intelligence on top of K* to make it more useful for de novo design workflows.

