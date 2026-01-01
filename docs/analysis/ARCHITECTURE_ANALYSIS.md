# OSPREY Architecture Analysis

## Call Flow: Python → Java → C++

### High-Level Architecture

```
┌─────────────┐
│   Python    │  User-facing scripts (examples/python.*/*.py)
│   Scripts   │  
└──────┬──────┘
       │ JPype (JVM bridge)
       ▼
┌─────────────┐
│    Java     │  Main application logic, algorithms, orchestration
│   / Kotlin  │  - A* search, COMETS, KStar algorithms
│             │  - Data structures (ConfSpace, EnergyMatrix)
│             │  - GUI (JavaFX/Kotlin)
└──────┬──────┘
       │ JNA (Java Native Access)
       ▼
┌─────────────┐
│     C++     │  Performance-critical computations
│   / CUDA    │  - Energy calculations (ConfEcalc)
│             │  - Deep copy/serialization (DeepCopy)
│             │  - GPU acceleration (CudaConfEcalc)
└─────────────┘
```

### 1. Python → Java Bridge: JPype

**Technology**: JPype1 (v1.5.0)
- **Location**: `src/main/python/osprey/jvm.py`
- **Purpose**: Allows Python to call Java code by embedding JVM

**How It Works**:
```python
# Python script calls:
import osprey
osprey.start()  # Starts JVM via JPype

# Inside osprey.start():
import jpype
jpype.startJVM(...)  # Launches Java Virtual Machine
c = jpype.JPackage('edu.duke.cs.osprey')  # Access Java packages
```

**Example Flow**:
```python
# examples/python.GMEC/findGMEC.py
import osprey
osprey.start()

strand = osprey.Strand('1CC8.ss.pdb')  # Calls Java: edu.duke.cs.osprey.confspace.Strand
confSpace = osprey.ConfSpace(strand)    # Calls Java: edu.duke.cs.osprey.confspace.ConfSpace
ecalc = osprey.EnergyCalculator(...)    # Calls Java: edu.duke.cs.osprey.energy.EnergyCalculator
```

**Key Files**:
- `src/main/python/osprey/__init__.py` - Python API wrapper functions
- `src/main/python/osprey/jvm.py` - JVM management via JPype
- `src/main/python/osprey/wraps.py` - Type conversions between Python/Java

**Important**: Python scripts **do NOT** call C++ directly. They go through Java first.

---

### 2. Java → C++ Bridge: JNA (Java Native Access)

**Technology**: JNA (v5.10.0)
- **Location**: `src/main/java/edu/duke/cs/osprey/energy/compiled/NativeConfEnergyCalculator.java`
- **Purpose**: Java calls C++ functions without JNI boilerplate

**How It Works**:
```java
// Java code:
private static class NativeLib {
    static {
        Native.register("ConfEcalc");  // Loads libConfEcalc.so/.dylib
    }
    
    public static native double calc_amber_eef1_f64(
        ByteBuffer conf_space,  // Memory buffer shared with C++
        int[] conf,
        ByteBuffer inters,
        ByteBuffer out_coords
    );
}

// Calls C++ function:
extern "C" {
    double calc_amber_eef1_f64(
        void* conf_space,
        int32_t* conf,
        void* inters,
        void* out_coords
    );
}
```

**Data Transfer**:
1. Java prepares data in `ByteBuffer` (contiguous memory)
2. Java passes buffer pointer to C++ via JNA
3. C++ reads/writes directly to buffer (no copying)
4. C++ returns result (energy value, status, etc.)

**C++ Libraries**:
- `ConfEcalc` - CPU energy calculator
- `CudaConfEcalc` - GPU energy calculator  
- `IntelConfEcalc` - Intel-optimized CPU calculator
- `DeepCopy` - Deep copy/serialization (new)

**Library Loading**:
- Libraries built by CMake
- Placed in `src/main/resources/{platform}/` (e.g., `linux-x86-64/`)
- JNA loads from classpath resources at runtime

---

## What Uses C++ vs Pure Java?

### Uses C++ (via JNA)

**Energy Calculations**:
- `NativeConfEnergyCalculator` - Calls `ConfEcalc` C++ library
- `CudaConfEnergyCalculator` - Calls `CudaConfEcalc` CUDA library
- Used by: Energy computation, minimization, conformation assignment

**Deep Copy/Serialization** (recent addition):
- `DeepCopyNative` - Calls `DeepCopy` C++ library
- Used for: Java object deserialization, avoiding stack overflow issues

**Only ~5-10 Java files** directly call C++ via JNA:
1. `energy/compiled/NativeConfEnergyCalculator.java`
2. `energy/compiled/CudaConfEnergyCalculator.java`
3. `tools/DeepCopyNative.java`
4. `coffee/Coffee.java` (partial)
5. `tools/GLibC.java` (system calls)

---

### Pure Java (No C++)

**Tree Search Algorithms** (Performance-critical, but still Java):
- `astar/ConfAStarTree.java` - A* search for conformations
- `astar/SeqAStarTree.java` - A* search for sequences
- `astar/comets/COMETSTree.java` - COMETS algorithm
- `astar/SMAStarTree.java` - Simplified Memory-Bounded A*
- **Why Java?** Complex data structures, object-oriented design, easier debugging

**KStar Algorithms**:
- `kstar/KStar.java` - K* partition function calculation
- `kstar/BBKStar.java` - Branch-and-Bound K*
- `kstar/MSKStar.java` - Multi-State K*
- `kstar/MARKStar.java` - MARK* algorithm
- **Why Java?** Algorithm complexity, integration with Java data structures

**GMEC Finding**:
- `gmec/GMECFinder.java` - Base interface
- `gmec/SimpleGMECFinder.java` - Basic GMEC search
- `gmec/DEEGMECFinder.java` - Dead-End Elimination
- Uses A* internally (which is Java)

**Energy Matrix Computation**:
- `ematrix/EnergyMatrix.java` - Pre-computed energy lookup table
- `ematrix/EPICMatrix.java` - **Causes stack overflow issues** (recursive Java)
- **Candidate for C++ port**: EPIC matrix computation

**Data Structures**:
- `confspace/ConfSpace.java` - Conformation space representation
- `confspace/ConfDB.java` - Conformation database
- `confspace/Sequence.java` - Sequence representation
- All pure Java

**Pruning Algorithms**:
- `pruning/SimpleDEE.java` - Dead-End Elimination
- `pruning/TransitivePruning.java` - Transitive pruning
- `pruning/PLUG.java` - PLUG pruning
- Pure Java

**I/O & Utilities**:
- `structure/PDBIO.java` - PDB file I/O
- `structure/OMOLIO.java` - OMOL file I/O
- `tools/ObjectIO.java` - Serialization (has issues)
- Pure Java

**GUI**:
- `gui/` package (Kotlin/JavaFX)
- Pure Java/Kotlin, no C++

---

## Statistics: Java vs C++

### Java Code
- **Total Java files**: ~831 files
- **Test files**: ~150+ files
- **Pure Java algorithms**: ~95% of codebase
- **Files calling C++**: ~5-10 files (<1%)

### C++ Code
- **C++ modules**: 4 (ConfEcalc, IntelConfEcalc, CudaConfEcalc, DeepCopy)
- **Total C++ files**: ~20-30 source files
- **Purpose**: Performance-critical energy calculations only

---

## C++ Porting Candidates

Based on a preference for C++ and performance requirements:

### High Priority

**1. EPIC Matrix Computation** (`ematrix/EPICMatrix.java`)
- **Why**: Causes stack overflow (recursive Java)
- **Benefit**: Iterative C++ implementation, better memory management
- **Complexity**: Medium (recursive → iterative transformation)
- **Impact**: High (fixes critical bugs)

**2. Energy Matrix Computation** (`ematrix/EnergyMatrix.java`)
- **Why**: Performance bottleneck, called millions of times
- **Benefit**: SIMD optimization, better cache locality
- **Complexity**: Medium-High
- **Impact**: High (performance improvement)

**3. A* Tree Search** (`astar/ConfAStarTree.java`)
- **Why**: Core algorithm, performance-critical
- **Benefit**: Better memory layout, pool allocators, parallel search
- **Complexity**: High (complex algorithm, stateful)
- **Impact**: Very High (core performance)

**4. COMETS Algorithm** (`astar/comets/COMETSTree.java`)
- **Why**: Advanced search algorithm, performance-critical
- **Benefit**: Similar to A* benefits
- **Complexity**: Very High
- **Impact**: High

### Medium Priority

**5. Pruning Algorithms** (`pruning/`)
- **Why**: Called frequently, simple logic
- **Benefit**: SIMD optimization, faster execution
- **Complexity**: Low-Medium
- **Impact**: Medium

**6. ObjectIO.deepCopy()** (`tools/ObjectIO.java`)
- **Why**: Already partially done (DeepCopy C++ module)
- **Benefit**: Better performance, fixes stack overflow
- **Complexity**: Medium (already in progress)
- **Impact**: Medium

### Lower Priority (Keep in Java)

**7. KStar Algorithms** (`kstar/`)
- **Why**: Complex object-oriented design, less performance-critical
- **Recommendation**: Keep in Java (or hybrid approach)
- **Note**: Energy calculations already use C++, which is the bottleneck

**8. GUI** (`gui/`)
- **Why**: Cross-platform desktop application
- **Recommendation**: Keep in Java/Kotlin (JavaFX)

**9. File I/O** (`structure/PDBIO.java`, etc.)
- **Why**: Infrequent operations, well-tested
- **Recommendation**: Keep in Java

**10. Data Structures** (`confspace/`, etc.)
- **Why**: Complex, object-oriented, tightly integrated
- **Recommendation**: Keep in Java (or gradual port)

---

## Architecture Comparison

### C++-first Approach
- **Primary Language**: C++
- **Internal Code**: All in C++
- **Bindings**: Python bindings (likely pybind11)
- **Structure**: C++ core with Python API

### OSPREY's Current Approach
- **Primary Language**: Java
- **Internal Code**: ~95% Java
- **Performance Code**: C++ (energy calculations only)
- **Python**: Thin wrapper via JPype → Java → JNA → C++

### Migration Path for a C++ Port
1. **Phase 1**: Port performance-critical algorithms to C++
   - EPIC matrix (fixes bugs)
   - Energy matrix computation
   - A* search
   
2. **Phase 2**: Create direct Python → C++ bindings
   - Replace JPype → Java → JNA with pybind11 → C++
   - Remove Java dependency from Python API
   
3. **Phase 3**: Full C++ core
   - Port remaining algorithms
   - Keep Java only for GUI (if needed)

---

## Summary

### Current Architecture
- **Python** → **JPype** → **Java** → **JNA** → **C++** (two-layer bridge)
- Most code is pure Java
- C++ used only for energy calculations
- Python always goes through Java

### C++ Porting Opportunities
1. **High Impact**: EPIC matrix, Energy matrix, A* search, COMETS
2. **Medium Impact**: Pruning, Deep copy (in progress)
3. **Keep Java**: KStar algorithms, GUI, I/O, data structures

### Key Insight
**Most Java code does NOT touch C++**. Only ~5-10 files directly call C++ via JNA. The majority of the codebase (A*, KStar, COMETS, data structures) is pure Java and would be candidates for C++ porting in a C++-first architecture.

---

*Last Updated: 2025-01-XX*

