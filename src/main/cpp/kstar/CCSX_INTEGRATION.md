# .ccsx File Integration Strategy

## Current State

### .ccsx File Format
- **Format**: Binary, XZ-compressed (`.ccsx`) or uncompressed (`.ccs`)
- **Loader**: Java `ConfSpace.fromBytes()` in `edu.duke.cs.osprey.confspace.compiled.ConfSpace`
- **Python API**: `osprey.ccs.loadConfSpace(path)` → calls Java
- **C++ Access**: Via JNA - C++ gets read-only view of Java-allocated memory

### C++ ConfSpace Structure
**Location**: `src/main/cc/ConfEcalc/confspace.h`

**Key Points**:
- `ConfSpace()` constructor is deleted - "created only on the Java side"
- C++ code gets a pointer to Java-allocated memory
- Memory layout is Java-compatible (for JNA)
- Read-only accessors (all methods are `const`)

**Structure**:
```cpp
template<std::floating_point T>
class ConfSpace {
    // Memory layout defined by Java side
    // Offset-based access (no pointers, cache-friendly)
    // Read-only (const methods only)
    
    const Pos& get_pos(int posi) const;
    const Conf<T>& get_conf(const Pos& pos, int confi) const;
    const Array<Real3<T>>& get_static_atom_coords() const;
    // ... other accessors
};
```

## Integration Options

### Option 1: Use JNA (Recommended for Phase 1)

**Approach**: Load via Java, pass to C++ via JNA

**Pros**:
- Reuses existing Java loader
- No need to reimplement .ccsx parser
- Works with existing test files
- Minimal code changes

**Cons**:
- Requires JVM/JNA dependency
- Not pure C++ (but acceptable for Phase 1)

**Implementation**:
```cpp
// C++ side: Accept ConfSpace pointer from Java
template<std::floating_point T>
PartitionFunctionResult<T> PartitionFunction<T>::compute(
    const Sequence& seq,
    const ConfSpace<T>* confspace,  // From Java via JNA
    T epsilon
);
```

**Python bridge**:
```python
# Load via Java
confspace_java = osprey.ccs.loadConfSpace("complex.ccsx")

# Get native pointer (JNA)
confspace_ptr = confspace_java.getNativePointer()  # If such method exists

# Pass to C++
result = cpp_kstar.compute(seq, confspace_ptr, epsilon)
```

### Option 2: Pure C++ .ccsx Loader (Future)

**Approach**: Implement C++ .ccsx parser

**Pros**:
- Pure C++ (no Java dependency)
- Better for MPI/multi-node (no JVM per node)
- More control over memory layout

**Cons**:
- Significant implementation work
- Need to reverse-engineer .ccsx format
- Duplicate code (Java loader already exists)

**When to implement**:
- Phase 3 (MPI) if JNA becomes bottleneck
- If we need pure C++ for cluster deployment

## Test Data Available

### Test .ccsx Files
**Location**: `src/test/resources/confSpaces/`

**Small test cases**:
- `dipeptide.5hydrophobic.ccsx` - Minimal test case
- `6ov7.tiny.*.ccsx` - Tiny protein (protein, ligand, complex)
- `6ov7.small.*.ccsx` - Small protein
- `6ov7.medium.*.ccsx` - Medium protein

**Real examples**:
- `2RL0.*.ccsx` - Actual protein from pipeline
- `1dg9.6f.*.ccsx` - Another real example

**Pipeline examples**:
- `examples/python.ccs/kstar/*.ccsx` - ptpase, hepes, complex

### Java Test Infrastructure

**K* Tests**:
- `src/test/java/edu/duke/cs/osprey/kstar/TestKStar.java` - Comprehensive K* tests
- `src/test/java/edu/duke/cs/osprey/kstar/compiled/TestKStar.java` - Compiled confspace tests

**Test Patterns**:
```java
// Load confspace
ConfSpace confSpace = ConfSpace.fromBytes(FileTools.readFileBytes("complex.ccsx"));

// Configure K*
KStar kstar = new KStar(protein, ligand, complex, settings);

// Run and validate
List<ScoredSequence> scores = kstar.run(tasks);
assertSequence(scores, expected);
```

## Recommended Approach

### Phase 1: JNA Integration

1. **Use existing Java loader** via Python bridge
2. **Pass ConfSpace pointer** to C++ via JNA
3. **Test with small .ccsx files** from `src/test/resources/confSpaces/`
4. **Compare results** with Java K* implementation

**Benefits**:
- Fastest path to working implementation
- Reuses existing, tested code
- Can validate correctness immediately

### Phase 2: Evaluate Pure C++ Loader

**Decision criteria**:
- If JNA becomes bottleneck → implement C++ loader
- If MPI needs pure C++ → implement C++ loader
- Otherwise → keep JNA approach

## Test Strategy

### Unit Tests (Current)
- Sequence, ThreadPool - Done
- Need: ConfSpace loading test

### Integration Tests (Next)
1. **Load small .ccsx file** (dipeptide.5hydrophobic.ccsx)
2. **Verify ConfSpace structure** (positions, conformations)
3. **Test PartitionFunction placeholder** with real ConfSpace
4. **Compare with Java** K* results

### Correctness Tests (Future)
1. **Use test .ccsx files** from `src/test/resources/confSpaces/`
2. **Run Java K*** on same files
3. **Run C++ K*** on same files
4. **Compare results** (K* scores should match)

## Next Steps

1. **Investigate JNA interface** for ConfSpace pointer access
2. **Create ConfSpace loading test** using small .ccsx file
3. **Implement minimal PartitionFunction** that uses ConfSpace
4. **Add integration test** comparing with Java results
