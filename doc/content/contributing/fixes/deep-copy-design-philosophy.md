---
title: "Deep Copy Design Philosophy"
date: 2025-12-07
weight: 2
---

# Deep Copy Design Philosophy

## Overview

The C++ deep copy implementation parses Java serialization streams to reconstruct object graphs iteratively, avoiding the stack overflow issues of Java's recursive `ObjectIO.deepCopy()`.

## Design Approach: Structure Parsing vs. Object Reconstruction

### Current Implementation: Structure Parsing

The current C++ implementation **parses the stream structure** but does **not reconstruct Java objects in C++**. Instead:

1. **Parse stream format** - Read Java serialization tags, class descriptors, field types
2. **Track object graph** - Build a graph of object relationships and references
3. **Handle cycles** - Detect and preserve circular references
4. **Skip primitives** - Read primitive values to advance stream offset, but don't store them
5. **Handle arrays** - Parse array structures (primitive arrays, object arrays, multi-dimensional arrays)

### Why This Approach?

**Java serialization format** (as used by `ObjectOutputStream`/`ObjectInputStream` in `ObjectIO.deepCopy()`) writes:
- **Primitive fields** directly as bytes (byte, char, short, int, long, float, double, boolean)
- **Object fields** as stream tags (TC_OBJECT, TC_REFERENCE, TC_NULL, etc.)
- **Array fields** as TC_ARRAY followed by class descriptor, length, and elements

Since parsing a **Java serialization stream**, must handle all 8 primitive types that Java supports:
- `B` = byte (1 byte)
- `C` = char (2 bytes, UTF-16)
- `D` = double (8 bytes)
- `F` = float (4 bytes)
- `I` = int (4 bytes)
- `J` = long (8 bytes)
- `S` = short (2 bytes)
- `Z` = boolean (1 byte)

### Primitive Handling: Read but Don't Store

```cpp
// Current implementation: Just skip over primitives
case 'I':  // int
    readInt(currentOffset);  // Read to validate stream integrity
    currentOffset += 4;      // Advance past it
    break;
```

**Why not store primitives?**
- Parsing structure, not reconstructing objects
- Primitives will be preserved when serialize back to Java (round-trip)
- No need to allocate C++ storage for values not used

### Smart Pointers: Yes, Templates: Not Yet

**Smart Pointers:**
- `std::unique_ptr<void, ObjectGraphDeleter>` for object ownership
- Automatic memory management for the object graph
- Prevents leaks when deserialization fails

**Templates: Not Used for Primitives**
- Don't store primitive values, so no need for `template<typename T> T readPrimitive()`
- Simple switch statement is sufficient for skipping bytes
- Templates would add complexity without benefit

**Future: Full C++ Implementation**

If building a full C++ object representation, then templates become relevant:

```cpp
// Future: If reconstruct objects in C++
template<typename T>
T readPrimitive(size_t offset) {
    // Read and return actual value
    // Store in C++ object representation
}

// Type-specific object reconstruction
template<typename JavaType>
std::unique_ptr<CppRepresentation<JavaType>> deserializeObject(...);
```

This would require:
- Type mapping from Java classes to C++ types
- Storage for all primitive values
- C++ object construction
- More complex memory management

**Current approach is simpler** and sufficient for the goal: iterative deep copy without stack overflow.

## Memory Management

### Object Graph Ownership

```cpp
using ObjectGraphPtr = std::unique_ptr<void, ObjectGraphDeleter>;

struct DeserializationNode {
    ObjectGraphPtr object;           // Owned object data
    std::vector<size_t> references;  // Node indices (not owned)
    size_t classId;                  // Class descriptor handle
    size_t handleId;                 // Java serialization handle
};
```

**Key Points:**
- Each node owns its object data via `unique_ptr`
- References are indices, not pointers (avoid ownership issues)
- Custom deleter handles cleanup of object graph

## Reference: Java Implementation

The original Java code (`ObjectIO.deepCopy()`) uses recursive serialization:

```java
// From ObjectIO.java:285
static public Object deepCopy(Object oldObj) {
    ByteArrayOutputStream bos = new ByteArrayOutputStream();
    ObjectOutputStream oos = new ObjectOutputStream(bos);
    oos.writeObject(oldObj);   // Recursive serialization
    oos.flush();
    ByteArrayInputStream bin = new ByteArrayInputStream(bos.toByteArray());
    ObjectInputStream ois = new ObjectInputStream(bin);
    Object ans = ois.readObject();  // Recursive deserialization
    return ans;
}
```

**Problems:**
- `writeObject()` and `readObject()` are recursive
- Stack overflow on deep object graphs
- Requires increasing JVM stack size (`-Xss`)

**This Solution:**
- Java serializes once (recursive, but only once)
- C++ deserializes iteratively (no recursion)
- Avoids stack overflow during deserialization

## Array Handling

Arrays in Java serialization are represented as `TC_ARRAY` tags followed by:
1. **Class descriptor** - Describes the array type (e.g., `[I` for `int[]`, `[Ljava/lang/String;` for `String[]`)
2. **Array length** - 4-byte integer
3. **Array elements** - Primitives written directly, objects/arrays as stream tags

**Implementation:**
- **Primitive arrays** - Read length, skip elements based on primitive type size
- **Object arrays** - Read length, process each element as a stream tag (TC_NULL, TC_REFERENCE, TC_OBJECT, TC_ARRAY, TC_STRING)
- **Multi-dimensional arrays** - Recursively handle nested arrays (e.g., `int[][]` is `[[I`)
- **Empty arrays** - Handle length 0 correctly
- **Array references** - Support TC_REFERENCE for class descriptors when arrays share types

**Skipping arrays:**
- When skipping (in `skipObjectData`), determine element type from class descriptor
- For forward references (class descriptor not yet parsed), use heuristic: peek at first element to detect primitives vs objects
- Recursively skip nested arrays and objects

## Summary

1. **Parse structure, don't reconstruct objects** - Simpler, sufficient for goal
2. **Handle all 8 primitives** - Required by Java serialization format
3. **Handle arrays** - Primitive arrays, object arrays, multi-dimensional arrays
4. **Use smart pointers** - Automatic memory management
5. **No templates for primitives** - Not needed when just skipping bytes
6. **Future: Full C++ implementation** - Would use templates, but not required now

The design prioritizes **simplicity and correctness** over full object reconstruction, which aligns with the primary goal: **iterative deep copy without stack overflow**.

