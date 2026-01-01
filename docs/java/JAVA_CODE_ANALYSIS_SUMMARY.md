# Java Code Analysis Summary

## Code Statistics

### Lines of Code (cloc)
- **Java**: 662 files, 97,584 lines
- **Kotlin**: 171 files, 19,050 lines
- **Python**: 27 files, 7,221 lines
- **Total Main Source**: 123,984 lines

### Language Distribution

| Language | Files | Lines | Percentage |
|----------|-------|-------|------------|
| Java | 662 | 97,584 | 78.7% |
| Kotlin | 171 | 19,050 | 15.4% |
| Python | 27 | 7,221 | 5.8% |
| **Java + Kotlin** | **833** | **116,634** | **94.1%** |
| C++ | ~20 | ~3,583 | ~2.9% |

**Conclusion**: Java/Kotlin is the vast majority (94.1%). Python is a thin wrapper (5.8%). C++ is minimal (2.9%).

---

## Complexity Analysis (lizard)

### High Complexity Functions (>15 CCN)

Only 2 functions exceed CCN 15 threshold:

1. **COMETSTree::calcLBPartialSeq** (COMETSTree.java:494-607)
   - NLOC: 75
   - CCN: 24
   - Function: Calculate lower bound for partial sequence in COMETS algorithm

2. **NewCOMETSTree::calcLBPartialSeq** (NewCOMETSTree.java:496-609)
   - NLOC: 75
   - CCN: 24
   - Function: Same as above, newer implementation

### Medium-High Complexity Functions (10-15 CCN)

From sample analysis:
- `COMETSTree::boundStateNonMutE` - CCN 14
- `COMETSTree::updateUB` - CCN 12
- `COMETSTree::calcLBConfTrees` - CCN 11
- `NodeUpdater::update` - CCN 16
- `LinkedConfAStarNode::index` - CCN 11

### Comparison to C++

**C++ High Complexity**:
- `skipObjectData` - CCN 52 (very high)
- `processClassDesc` - CCN 30
- `line_search_surf` - CCN 28

**Java High Complexity**:
- `calcLBPartialSeq` - CCN 24 (highest in Java)

**Observation**: Java code has lower complexity than C++ deserializer. Most Java functions are well-structured.

---

## Key Java Components by Size

### Largest Files (from cloc output)
- Core algorithms: A* search, COMETS, KStar
- Energy calculations: Forcefield, energy matrices
- Data structures: ConfSpace, Sequence, ConfDB
- I/O: PDBIO, OMOLIO, serialization

### Algorithm Complexity

**A* Search** (`astar/conf/ConfAStarTree.java`):
- Core tree search algorithm
- Medium complexity (most functions CCN < 10)
- Well-structured with clear separation of concerns

**COMETS** (`astar/comets/COMETSTree.java`):
- Multi-state design algorithm
- Highest complexity in codebase (CCN 24)
- Nested search (sequences × conformations)

**KStar** (`kstar/KStar.java`):
- Partition function calculation
- Medium complexity
- Statistical mechanics computations

**EPIC Matrix** (`ematrix/epic/EPICMatrix.java`):
- Energy polynomials
- Recursive implementation (causes stack overflow)
- Similar complexity to DeepCopy deserializer

---

## Python Role

### Python Usage
- **27 files, 7,221 lines** (5.8% of codebase)
- **Purpose**: User-facing API wrapper
- **Technology**: JPype (Python → Java bridge)
- **Function**: Syntactic sugar for Java API

### Python Files
- `osprey/__init__.py` - Main API
- `osprey/jvm.py` - JVM management via JPype
- `osprey/wraps.py` - Type conversions
- Example scripts in `examples/python.*/`

### Python Does NOT:
- Call C++ directly
- Implement core algorithms
- Perform heavy computation

### Python Does:
- Provide convenient syntax for users
- Launch JVM and call Java classes
- Convert Python types to Java types
- Script workflows

**Conclusion**: Python is purely syntactic sugar. All computation happens in Java/C++.

---

## Porting Implications

### Current State
- **94.1% Java/Kotlin** - Core implementation
- **2.9% C++** - Performance-critical energy calculations
- **5.8% Python** - User API wrapper

### For C++ Migration

**Phase 1: Port Algorithms**
- A* search (medium complexity, core algorithm)
- COMETS (high complexity, but manageable)
- EPIC matrix (fixes bugs, similar to DeepCopy)
- Energy matrix computation (performance)

**Phase 2: Replace Python API**
- Replace JPype → Java → JNA → C++
- With pybind11 → C++ (direct)
- Keep Python as user API, but call C++ directly

**Phase 3: Full C++ Core**
- Port remaining Java algorithms
- Keep Java only for GUI (if needed)

---

## Code Quality Observations

### Strengths
- Well-modularized Java code
- Lower complexity than C++ deserializer
- Clear separation: algorithms, data structures, I/O
- Comprehensive test coverage

### Areas for Improvement
- COMETS `calcLBPartialSeq` (CCN 24) could be refactored
- EPIC matrix recursive implementation (stack overflow)
- Some long methods (75+ NLOC)

### Porting Readiness
- Java code is generally well-structured
- Algorithms are clear candidates for C++ porting
- Complexity is manageable (CCN 24 max vs C++ CCN 52)

---

## Summary

**Language Distribution**:
- Java/Kotlin: 94.1% (core implementation)
- Python: 5.8% (syntactic sugar wrapper)
- C++: 2.9% (performance-critical)

**Python Role**: Pure syntactic sugar. All computation in Java/C++.

**Complexity**: Java code is well-structured. Highest complexity (CCN 24) is lower than C++ deserializer (CCN 52).

**Porting**: Java algorithms are good candidates for C++ porting. Complexity is manageable.

---

*Analysis Date: 2025-01-XX*
*Tools Used: cloc 1.90, lizard 1.19.0*

