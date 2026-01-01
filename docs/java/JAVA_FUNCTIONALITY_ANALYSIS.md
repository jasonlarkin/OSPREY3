# Pure Java Functionality Analysis

## Overview

Pure Java components (not using C++) implement core algorithms, data structures, and orchestration. These are the primary candidates for C++ porting to align with a C++-first architecture.

---

## Core Algorithms

### 1. A* Search (`astar/conf/ConfAStarTree.java`)

**Function**: Tree search to find lowest-energy conformations in discrete conformation space.

**How it works**:
- Represents conformation space as tree: root → positions → conformations
- Each path from root to leaf = unique conformation
- Scores nodes using G-score (assigned positions) + H-score (heuristic for unassigned)
- Expands nodes in priority queue (lowest score first)
- Returns conformations sorted by lower bound on energy

**Computational task**: Graph traversal with heuristic scoring. Processes millions of nodes.

**Calls C++**: No. Pure Java algorithm.

**Performance bottleneck**: H-score calculation (polynomial time), memory-intensive for large spaces.

**Porting benefit**: Better memory layout, pool allocators, parallel search.

---

### 2. COMETS Algorithm (`gmec/Comets.java`, `astar/comets/COMETSNode.java`)

**Function**: Multi-state design. Finds best sequences considering multiple protein states (bound/unbound, etc.).

**How it works**:
- Searches over sequence space using A* on sequences
- For each sequence, searches conformation space
- Computes objective function across multiple states
- Uses A* tree search internally (`SeqAStarTree`)

**Computational task**: Nested search (sequences × conformations).

**Calls C++**: No. Pure Java algorithm.

**Performance bottleneck**: Nested search spaces, multiple state evaluations.

**Porting benefit**: Similar to A*, with multi-state complexity.

---

### 3. KStar Algorithm (`kstar/KStar.java`)

**Function**: Computes binding affinity predictions using partition functions.

**How it works**:
- Computes partition function Q = Σ exp(-E_i/kT) for three states:
  - Protein (unbound)
  - Ligand (unbound)  
  - Complex (bound)
- Calculates K* = Q_complex / (Q_protein × Q_ligand)
- Uses A* search internally to enumerate conformations
- Boltzmann-weighted ensemble averaging

**Computational task**: Statistical mechanics partition function calculation.

**Calls C++**: No. Pure Java algorithm, but uses A* which could use C++ energy calculations.

**Performance bottleneck**: Partition function enumeration, BigDecimal arithmetic.

**Porting benefit**: Medium priority - complex object-oriented design, but high computational cost.

---

### 4. Energy Matrix (`ematrix/EnergyMatrix.java`)

**Function**: Pre-computed lookup table for pairwise conformation energies.

**How it works**:
- Stores E(pos_i, conf_i, pos_j, conf_j) for all pairs
- Pre-computed once, then used millions of times in A* search
- Used for G-score and H-score calculations

**Computational task**: Table lookup and aggregation.

**Calls C++**: No. Pure Java data structure.

**Performance bottleneck**: Memory access patterns, cache locality.

**Porting benefit**: High - SIMD optimization, better memory layout.

---

### 5. EPIC Matrix (`ematrix/epic/EPICMatrix.java`)

**Function**: Energy polynomials in internal coordinates for continuous flexibility.

**How it works**:
- Stores polynomial energy functions (not just discrete values)
- Allows continuous minimization over degrees of freedom
- Used with EnergyMatrix for full energy calculation

**Computational task**: Polynomial evaluation and minimization.

**Calls C++**: No. Pure Java, recursive implementation causes stack overflow.

**Performance bottleneck**: Stack overflow in deep recursion, polynomial evaluation.

**Porting benefit**: Very high - fixes critical bugs, iterative implementation.

---

## Data Structures

### Conformation Space (`confspace/`)

**Function**: Represents molecular conformational flexibility.

**Components**:
- `SimpleConfSpace.java` - Discrete conformation space
- `ConfSpace.java` - Compiled representation
- `Conf.java` - Individual conformation
- `Sequence.java` - Amino acid sequences

**Calls C++**: No. Pure Java data structures.

---

### Pruning Matrices (`pruning/`)

**Function**: Pre-computed matrices to eliminate conformations that can't be optimal.

**Algorithms**:
- `SimpleDEE.java` - Dead-End Elimination
- `TransitivePruning.java` - Transitive closure pruning
- `PLUG.java` - PLUG pruning

**Calls C++**: No. Pure Java.

**Performance**: Called frequently, simple logic.

**Porting benefit**: Medium - SIMD optimization possible.

---

## Orchestration & Workflows

### GMEC Finding (`gmec/`)

**Function**: Finds Global Minimum Energy Conformation.

**Components**:
- `SimpleGMECFinder.java` - Basic search
- `DEEGMECFinder.java` - With dead-end elimination
- Uses A* internally

**Calls C++**: No. Orchestrates Java algorithms.

---

### File I/O (`structure/`)

**Function**: Reads/writes molecular structure files.

**Components**:
- `PDBIO.java` - PDB file format
- `OMOLIO.java` - OSPREY molecular format

**Calls C++**: No. Pure Java I/O.

---

### Serialization (`tools/ObjectIO.java`)

**Function**: Deep copy via Java serialization.

**Calls C++**: Partially - DeepCopy C++ module exists but not fully integrated.

**Status**: Has stack overflow issues. C++ port in progress.

---

## Summary: Functional Categories

| Component | Function | Calls C++? | Port Priority |
|-----------|----------|------------|---------------|
| A* Search | Tree search for conformations | No | High |
| COMETS | Multi-state sequence design | No | High |
| KStar | Binding affinity prediction | No | Medium |
| Energy Matrix | Energy lookup table | No | High |
| EPIC Matrix | Continuous energy polynomials | No | Very High (bugs) |
| Pruning | Conformation elimination | No | Medium |
| Data Structures | ConfSpace, Sequence, etc. | No | Low-Medium |
| GMEC Finding | Orchestrates search | No | Low (uses A*) |
| File I/O | PDB/OMOL reading | No | Low |

---

## Key Insight

**Most computation is in algorithms, not data structures:**
- A* search: Graph traversal, heuristic calculation
- COMETS: Nested search
- KStar: Partition function enumeration
- Energy/EPIC matrices: Polynomial evaluation, lookup

**These are computational kernels**, not just data management. C++ porting would target these compute-intensive loops, not just memory layout.

---

*Last Updated: 2025-01-XX*

