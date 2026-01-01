# C++ Implementation Inventory - OSPREY Fork

## Overview

This document provides a comprehensive inventory of all C++ implementations in the OSPREY codebase, analyzing what they are, what computations they perform, and what C++ features are used.

**C++ Standard:** C++17 (configured in `CMakeLists.txt` with `set(CMAKE_CXX_STANDARD 17)`)

---

## 1. ConfEcalc - CPU Energy Calculator

**Location:** `src/main/cc/ConfEcalc/`

### What It Is

A native C++ library for computing molecular conformation energies using the Amber/EFF1 force field. This is the performance-critical component that calculates interaction energies between atoms in protein conformations.

**Interface:** JNA (Java Native Access) - called from Java via `NativeConfEnergyCalculator.java`

**API Functions (C interface for JNA):**
- `version_major()` / `version_minor()` - Version information
- `assign_f32/f64()` - Assign conformations to positions
- `calc_amber_eef1_f32/f64()` - Calculate energy for a conformation
- `minimize_amber_eef1_f32/f64()` - Minimize energy using CCD (Cyclic Coordinate Descent)

### Computations Performed

1. **Conformation Assignment** (`assign_f32/f64`)
   - Builds 3D atomic coordinates from conformation space
   - Combines static atoms with rotamer positions
   - Creates `Assignment<T>` structure with atom coordinates and pair data

2. **Energy Calculation** (`calc_amber_eef1_f32/f64`)
   - **Amber Force Field Interactions:**
     - Electrostatic energy: `esQ/r` or `esQ/r²` (distance-dependent dielectric)
     - Van der Waals energy: `vdwA/r¹² - vdwB/r⁶` (Lennard-Jones potential)
   - **EEF1 (Effective Energy Function 1) Interactions:**
     - Implicit solvation energy: `-(α₁e^(-Xᵢⱼ²) + α₂e^(-Xⱼᵢ²))/r²`
     - Where `Xᵢⱼ = (r - vdwRadius₁)/λ₁`
   - Computes pairwise interactions between all atom pairs in the conformation
   - Uses position-based indexing for efficient pair lookup

3. **Energy Minimization** (`minimize_amber_eef1_f32/f64`)
   - **CCD (Cyclic Coordinate Descent) Algorithm:**
     - Iteratively optimizes degrees of freedom (DOFs)
     - Performs line search along each DOF dimension
     - Uses quadratic approximation with "surfing" to find local minima
     - Handles dihedral angles and translation-rotation motions
   - **Line Search Algorithm:**
     - Fits quadratic model: `q(x) = fx + a*(x - xd)² + b*(x - xd)`
     - "Surfs" along energy landscape to find better minima
     - Includes 1-degree step search to jump over energy barriers
   - Converges when energy improvement < 0.001 or max 30 iterations

4. **Molecular Motions**
   - **Dihedral Rotations:**
     - Rotates atoms around a bond axis (dihedral angle)
     - Uses `Rotation<T>` matrices for coordinate transformations
     - Tracks which positions are modified by the rotation
   - **Translation-Rotation Motions:**
     - Rigid body transformations (translation + rotation)
     - Applies to entire molecules or molecular fragments
     - Uses rotation matrices and translation vectors

5. **Rotation Mathematics**
   - 3x3 rotation matrices (Euler angles)
   - X/Y/Z axis rotations: `set_x()`, `set_y()`, `set_z()`
   - Combined rotations: `set_xyz()` for all three axes
   - Matrix-vector multiplication: `apply()` to transform coordinates
   - Angle normalization: `normalize_mpi_pi()` to [-π, π] range

### Key Data Structures

- **`ConfSpace<T>`** - Conformation space (positions, rotamers, atom coordinates)
- **`Assignment<T>`** - Specific conformation assignment with atomic coordinates
- **`Real3<T>`** - 3D vector (x, y, z) with vector operations
- **`Array<T>`** - Fixed-length array with Java/C++ dual allocation modes
- **`PosInter<T>`** - Position interaction with weight and offset
- **`Dofs<T>`** - Degrees of freedom for minimization
- **`DofValues<T>`** - Current DOF values and energy
- **`Rotation<T>`** - 3x3 rotation matrix (row vectors: xaxis, yaxis, zaxis)
- **`Dof<T>`** - Abstract base class for degrees of freedom
- **`motions::Dihedral<T>`** - Dihedral angle rotation motion
- **`motions::TranslationRotation<T>`** - Translation-rotation motion for rigid bodies

### C++ Features Used

#### Templates
- **Primary Use:** Generic programming for `float32_t` and `float64_t` precision
- **Template Classes:**
  - `Array<T>` - Generic array container
  - `Real3<T>` - 3D vector templated on numeric type
  - `ConfSpace<T>` - Conformation space templated on precision
  - `Assignment<T>` - Assignment templated on precision
  - `Dofs<T>`, `DofValues<T>` - Minimization structures
  - `PosInter<T>`, `AtomPairAmber<T>`, `AtomPairEef1<T>` - Energy computation structures
- **Template Functions:**
  - `osprey::assign<T>()` - Generic assignment function
  - `osprey::calc<T>()` - Generic energy calculation
  - `osprey::minimize<T>()` - Generic minimization
  - `distance_sq<T>()`, `distance<T>()` - Vector distance functions
  - `line_search_surf<T>()` - Line search algorithm
- **Template Specializations:**
  - `tolerance<T>` - Specialized for `float32_t` (1e-3) and `float64_t` (1e-6)

#### Modern C++17 Features

1. **Move Semantics**
   - **`Array<T>`** has move constructor and move assignment operator
   - **Critical for JNA:** Preserves Java-allocated memory mode (nullptr check)
   - Marked `noexcept` for exception safety

2. **Attributes**
   - **`[[nodiscard]]`** - On `Array::get_size()`, `Array::operator[]()` - indicates return value should not be ignored
   - **`[[maybe_unused]]`** - On `API` macro for functions that may be unused in some builds

3. **`noexcept` Specifications**
   - Version functions: `version_major() noexcept`, `version_minor() noexcept`
   - Move operations: `Array` move constructor/assignment `noexcept`
   - Accessors: `Array::get_size() const noexcept`

4. **`alignas` Specifications**
   - **`alignas(8)`** on `Pos`, `Real3<T>`, `Params`, `AtomPairs` - Ensures Java memory layout compatibility

5. **`constexpr`**
   - Used in template metaprogramming (e.g., `print_size_as_warning_char` in `global.h`)

6. **Lambda Functions**
   - Used in minimization: `auto f = [&dofs, d](T x) -> T { return dofs.eval_efunc(d, x); };`

7. **Function Pointers / Type Aliases**
   - `using EnergyFunction = T (*)(Assignment<T> &, const Array<PosInter<T>> &);`
   - `using LineSearchFunction = T (*)(Dofs<T> &, int, T, T &);`

8. **`std::clamp`** (C++17)
   - Used in line search: `xstar = std::clamp(xstar, xmin, xmax);`

#### Standard Library Usage

- **`<algorithm>`**: `std::copy`, `std::clamp`
- **`<cmath>`**: `std::sqrt`, `std::pow`, `std::exp`, `std::isnan`, `std::isinf`, `std::numeric_limits`
- **`<cstring>`**: `std::memset`
- **`<iostream>`**: `std::cout`, `std::cerr` (debug output)
- **`<stdexcept>`**: `std::invalid_argument`, `std::runtime_error`
- **`<memory>`**: Raw pointers (no smart pointers in ConfEcalc - uses manual memory management for compatibility)

#### Memory Management

- **Dual Allocation Modes:**
  - Java-allocated: `things == nullptr`, data follows class layout (`reinterpret_cast`)
  - C++-allocated: `things` points to heap memory (`new T[size]`)
- **Manual Memory Management:**
  - Raw pointers (`T* things`)
  - `new[]` / `delete[]` in constructors/destructors
  - Manual cleanup in `Assignment<T>`, `Dofs<T>` destructors

#### Object-Oriented Features

1. **Virtual Functions**
   - **`Dof<T>`** abstract base class with pure virtual methods:
     - `virtual T get() const = 0;` - Get current DOF value
     - `virtual void set(T val) = 0;` - Set DOF value
   - Polymorphic behavior for different motion types

2. **Inheritance Hierarchy**
   - Base: `Dof<T>` (abstract)
   - Derived: `motions::Dihedral<T>`, `motions::TranslationRotation<T>`

#### Design Patterns

- **Template Method Pattern:** Generic functions that work with both `float32_t` and `float64_t`
- **Strategy Pattern:** `EnergyFunction` and `LineSearchFunction` function pointers
- **RAII:** Destructors handle cleanup (`Array`, `Assignment`, `Dofs`)
- **Polymorphism:** Virtual functions in `Dof<T>` for motion types

---

## 2. IntelConfEcalc - Intel-Optimized Energy Calculator

**Location:** `src/main/cc/IntelConfEcalc/`

### What It Is

An Intel-optimized version of the CPU energy calculator. Similar structure to `ConfEcalc` but with Intel-specific optimizations (likely SIMD, AVX2/AVX-512).

### Structure

- Same header files as `ConfEcalc` (likely shared or duplicated)
- Similar API interface
- Optimized implementation details (not fully analyzed in this inventory)

### C++ Features

- Similar to `ConfEcalc` (templates, C++17 features)
- Likely includes SIMD intrinsics for vectorization

---

## 3. DeepCopy - Iterative Deep Copy Deserializer [OUR FORK ADDITION]

**Location:** `src/main/cc/DeepCopy/`

**NOTE:** This component was **developed in this fork** as part of the Java->C++ port work. It is **NOT** part of the original OSPREY codebase.

### What It Is

A C++ implementation of an iterative, non-recursive deep copy mechanism for Java serialized objects. Replaces Java's recursive `ObjectIO.deepCopy()` to avoid `StackOverflowError` for deep object graphs.

**Problem Solved:** Java serialization causes stack overflow for deeply nested objects (e.g., `MoleculeModifierAndScorer` in `SAPE.java:93`)

**Solution:** Iterative BFS (Breadth-First Search) traversal using a work queue instead of recursion

**Interface:** JNA via `DeepCopyNative.java`
- `deepCopyFromBuffer(ByteBuffer, long)` - Deserialize Java object
- `freeDeepCopy(Pointer)` - Free deserialized object
- `getLastError()` - Get error message

### Computations Performed

1. **Java Serialization Stream Parsing**
   - Reads Java serialization protocol binary format
   - Parses stream header (magic number, version)
   - Handles stream tags: `TC_NULL`, `TC_REFERENCE`, `TC_OBJECT`, `TC_ARRAY`, `TC_STRING`, `TC_CLASSDESC`, etc.

2. **Object Graph Construction (BFS)**
   - Uses work queue for iterative traversal
   - Processes nodes in breadth-first order
   - Builds object graph structure with reference tracking
   - Handles circular references via handle table

3. **Class Descriptor Processing**
   - Reads class names (with string interning support)
   - Parses field descriptors (type codes, field names)
   - Handles annotations and serial version UID
   - Builds class descriptor map for reference resolution

4. **Reference Resolution**
   - Two-pass algorithm:
     - **Pass 1:** Build object graph, track references
     - **Pass 2:** Resolve all references to actual object pointers
   - Handles forward references and circular structures

5. **Memory Management**
   - Allocates deserialized objects
   - Manages object graph lifetime with custom deleters
   - Tracks object handles for Java serialization protocol

### Key Data Structures

- **`IterativeDeserializer`** - Main deserializer class
- **`DeserializationNode`** - Node in object graph
- **`DeserializationTask`** - Work queue item (node index + stream offset)
- **`ClassDescriptor`** - Class metadata (name, fields, serialVersionUID)
- **`FieldDescriptor`** - Field metadata (type, name)
- **`ObjectGraphNode`** - Object graph representation
- **`ObjectGraphPtr`** - `std::unique_ptr<void, ObjectGraphDeleter>`

### C++ Features Used

#### Smart Pointers

1. **`std::unique_ptr`**
   - **`ObjectGraphPtr`** - `std::unique_ptr<void, ObjectGraphDeleter>` for deserialized objects
   - **`nodes`** - `std::vector<std::unique_ptr<DeserializationNode>>` - Node storage
   - **`std::make_unique`** - Used extensively for node creation
   - **Custom Deleter:** `ObjectGraphDeleter` - Manages object graph cleanup

2. **Memory Management Strategy:**
   - Owned by `std::unique_ptr` for automatic cleanup
   - Custom deleters for complex object graphs
   - Thread-local storage for deserializer instances (in `deepcopy.cc`)

#### Standard Library Containers

1. **`std::vector`**
   - `nodes` - Deserialization nodes
   - `references` - Reference indices in nodes
   - `referenceOffsets` - Stream offsets for references
   - `fields` - Field descriptors
   - `stringHandles` - String handle tracking

2. **`std::queue`**
   - `workQueue` - BFS traversal queue (`std::queue<DeserializationTask>`)

3. **`std::unordered_map`**
   - `handleToNode` - Map handle ID → node index
   - `offsetToNode` - Map stream offset → node index
   - `classDescriptors` - Map handle ID → class descriptor
   - `handleToString` - Map handle ID → string value (for string interning)

4. **`std::string`**
   - Used extensively for class names, field names, type names, error messages

#### Modern C++17 Features

1. **`enum class`**
   - `StreamTag` - Type-safe stream tag enumeration
   - Strongly typed, no implicit conversions

2. **Structured Bindings** (potential, though not seen in current code)
   - Could be used for map iteration

3. **`auto` Type Deduction**
   - Used for iterator types, lambda captures, etc.

4. **Lambda Functions**
   - Used in error handling and utility functions

5. **`noexcept`**
   - Not extensively used (error handling via exceptions)

6. **`constexpr`**
   - Potentially for compile-time constants

#### Exception Handling

- **`std::exception`** - Base exception class
- **`std::runtime_error`** - Used extensively for parsing errors
- **Error Propagation:** C++ exceptions caught and converted to error strings for JNA

#### Algorithm Design

1. **BFS (Breadth-First Search)**
   - Iterative traversal using work queue
   - Avoids recursion limits

2. **Two-Pass Algorithm**
   - Pass 1: Build object graph
   - Pass 2: Resolve references

3. **Handle Table Management**
   - Maps Java serialization handles to C++ objects
   - Supports forward references

#### Memory Safety

- **Bounds Checking:** Extensive validation of stream offsets
- **Null Checks:** Validation of pointers before use
- **RAII:** Automatic cleanup via smart pointers
- **Exception Safety:** Error handling with cleanup

---

## Summary: Original OSPREY vs. Fork Additions

### Original OSPREY Components
1. **ConfEcalc** - CPU energy calculator (original)
2. **IntelConfEcalc** - Intel-optimized version (original)
3. **CudaConfEcalc** - GPU calculator (original, location: `src/main/cu/`)

### Our Fork Additions
1. **DeepCopy** - Iterative deep copy deserializer (our addition)

**Key Insight:** The original OSPREY C++ codebase is relatively small and focused - primarily the energy calculation components. Our DeepCopy addition demonstrates the type of modernization work a C++ port would require.

## Summary: C++ Features Across Components

### Templates
- **ConfEcalc:** Extensive use for precision abstraction (`float32_t`/`float64_t`)
- **DeepCopy:** Limited use (mostly concrete types)

### Smart Pointers
- **ConfEcalc:** **None** (raw pointers for JNA compatibility)
- **DeepCopy:** **Extensive** (`std::unique_ptr` with custom deleters)

### Move Semantics
- **ConfEcalc:** Yes (`Array<T>` move constructor/assignment)
- **DeepCopy:** Yes (via `std::unique_ptr`)

### Standard Library Containers
- **ConfEcalc:** Minimal (`Array<T>` is custom, minimal STL)
- **DeepCopy:** Extensive (`std::vector`, `std::queue`, `std::unordered_map`, `std::string`)

### Attributes
- **ConfEcalc:** `[[nodiscard]]`, `[[maybe_unused]]`
- **DeepCopy:** Not extensively used

### Exception Handling
- **ConfEcalc:** Used (`std::invalid_argument`, `std::runtime_error`)
- **DeepCopy:** Extensive (error propagation for parsing)

### Memory Management Philosophy
- **ConfEcalc:** Manual (raw pointers, `new`/`delete`) for JNA compatibility
- **DeepCopy:** Modern (smart pointers, RAII)

### C++17 Features Summary
- Templates (extensive)
- `noexcept` specifications
- `[[nodiscard]]` attributes
- `alignas` specifications
- `std::clamp`
- `std::make_unique`
- `enum class`
- Lambda functions
- `auto` type deduction
- Move semantics
- Smart pointers (only in DeepCopy, not ConfEcalc due to JNA)
- `constexpr` (limited use)

### Design Patterns
- **Template Method Pattern:** Generic algorithms in ConfEcalc
- **Strategy Pattern:** Function pointers for energy/line search
- **RAII:** Resource management via constructors/destructors
- **Iterator Pattern:** Standard library containers
- **Factory Pattern:** Node creation in DeepCopy

---

## Build System

### CMake Configuration
- **C++ Standard:** C++17
- **Build Type:** Shared libraries for JNA
- **Testing:** Google Test framework (via `FetchContent`)
- **Platform:** Linux x86-64 (JNA platform-specific)

### Testing
- **ConfEcalc:** Google Test unit tests (`test_array.cpp`, `test_confecalc.cpp`)
- **DeepCopy:** Google Test unit + integration tests
  - Unit: `test_basic.cpp`, `test_stream_parser.cpp`, `test_class_descriptor.cpp`, etc.
  - Integration: `test_captured_data.cpp`, `test_1cc8_trace.cpp`

---

## Notes

1. **JNA Compatibility:** ConfEcalc uses raw pointers and manual memory management to ensure compatibility with JNA's memory layout requirements. DeepCopy uses modern smart pointers as it's purely C++-side.

2. **Dual Allocation Modes:** `Array<T>` supports both Java-allocated (nullptr check) and C++-allocated memory, critical for JNA integration.

3. **Template Abstraction:** Extensive use of templates allows single codebase for both `float32_t` and `float64_t` precision, reducing code duplication.

4. **Performance Focus:** ConfEcalc is optimized for numerical performance (vector operations, energy calculations), while DeepCopy focuses on memory safety and correctness.

5. **Error Handling:** ConfEcalc uses exceptions for internal errors, DeepCopy propagates errors to Java via JNA error strings.

