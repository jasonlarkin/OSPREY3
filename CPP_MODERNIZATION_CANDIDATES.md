# C++ Modernization Candidates - Specific Code Locations

## Template Usage Clarification

**Both `confecalc.cc` and `array.h` use templates extensively:**

- **`confecalc.cc`**: All main functions are templated (`template<typename T>`)
  - `assign<T>`, `calc<T>`, `minimize<T>` - 3 template functions
  - Specialized for `float32_t` and `float64_t` via explicit API functions

- **`array.h`**: Core container class is templated (`template<typename T> class Array`)
  - Used throughout ConfEcalc (103 matches across 12 files)
  - `Array<Real3<T>>`, `Array<PosInter<T>>`, `Array<T>`, `Array<int32_t>`

**Concepts would add compile-time constraints** to these existing templates, making them safer and more self-documenting.

---

## C++20 Features

### 1. Concepts

#### Candidate 1: Template Type Constraints

**Location**: `confecalc.cc:30-35`, `confecalc.cc:48-61`, `confecalc.cc:80-110`

**Current Code**:
```cpp
template<typename T>
static void assign(const ConfSpace<T> & conf_space, const int32_t conf[], Array<Real3<T>> & out_coords) {
    // T must be float32_t or float64_t, but no compile-time check
}

template<typename T>
static T calc(const ConfSpace<T> & conf_space, const int32_t conf[], ...) {
    // T must be floating point
}
```

**C++20 Modernization**:
```cpp
template<std::floating_point T>
static void assign(const ConfSpace<T> & conf_space, const int32_t conf[], Array<Real3<T>> & out_coords) {
    // Compiler enforces T is floating point
}

template<std::floating_point T>
static T calc(const ConfSpace<T> & conf_space, const int32_t conf[], ...) {
    // Compiler enforces T is floating point
}
```

**Expected Benefits**:
- **Stability**: Compile-time type checking prevents wrong template instantiation
- **Portability**: Clearer error messages across compilers
- **Performance**: Zero runtime overhead (compile-time only)
- **Maintainability**: Self-documenting code, better IDE support

---

#### Candidate 2: Array Template Constraints

**Location**: `array.h:10-11`

**Current Code**:
```cpp
template<typename T>
class Array {
    // T can be anything, but should be trivially copyable for performance
};
```

**C++20 Modernization**:
```cpp
template<std::trivially_copyable T>
class Array {
    // Compiler enforces T is trivially copyable
};
```

**Expected Benefits**:
- **Stability**: Prevents misuse with non-copyable types
- **Performance**: Ensures optimal copy operations
- **Maintainability**: Clearer intent

---

#### Candidate 3: Energy Function Constraints

**Location**: `energy.h:8-17`

**Current Code**:
```cpp
template<typename T>
using EnergyFunction = T (*)(Assignment<T> &, const Array<PosInter<T>> &);
```

**C++20 Modernization**:
```cpp
template<std::floating_point T>
concept EnergyFunction = std::invocable<T, Assignment<T>&, const Array<PosInter<T>>&> &&
                         std::same_as<std::invoke_result_t<T, Assignment<T>&, const Array<PosInter<T>>&>, T>;
```

**Expected Benefits**:
- **Stability**: Type-safe function pointer constraints
- **Maintainability**: Better error messages

---

### 2. `std::span` **HIGH IMPACT - Array Used Everywhere**

**Impact Analysis:**
- `Array<T>` is used **103 times across 12 files** in ConfEcalc
- Core data structure for all energy calculations
- Modernizing `Array` with `std::span` would improve:
  - **Stability**: Bounds checking in 100+ locations
  - **Performance**: Zero overhead, better compiler optimizations
  - **Maintainability**: Range-based for loops, clearer API

**Files using Array:**
- `confecalc.cc` (21 uses) - Main API functions
- `energy_ambereef1.h` (3 uses) - Energy calculations
- `minimization.h` (11 uses) - Minimization loops
- `confspace.h` (10 uses) - Conformation space access
- `assignment.h` (1 use) - Atom coordinates
- `motions.h` (8 uses) - Motion calculations
- Plus 6 more files

#### Candidate 1: Array Pointer Operations

**Location**: `array.h:100-118` - `pointer()` methods

**Current Code**:
```cpp
T * pointer() {
    if (things == nullptr) {
        return reinterpret_cast<T *>(this + 1);
    } else {
        return things;
    }
}

const T * pointer() const {
    if (things == nullptr) {
        return reinterpret_cast<const T *>(this + 1);
    } else {
        return things;
    }
}
```

**C++20 Modernization**:
```cpp
std::span<T> span() {
    if (things == nullptr) {
        return std::span<T>(reinterpret_cast<T *>(this + 1), size);
    } else {
        return std::span<T>(things, size);
    }
}

std::span<const T> span() const {
    if (things == nullptr) {
        return std::span<const T>(reinterpret_cast<const T *>(this + 1), size);
    } else {
        return std::span<const T>(things, size);
    }
}
```

**Expected Benefits**:
- **Stability**: Bounds checking in debug builds
- **Portability**: Standardized array view interface
- **Performance**: Zero overhead (just pointer + size wrapper)
- **Maintainability**: Range-based for loops, clearer API

---

#### Candidate 2: Energy Calculation Loops

**Location**: `energy_ambereef1.h:89-118` - `calc()` function

**Current Code**:
```cpp
template<typename T>
static T calc(const Array<Real3<T>> & atoms, const Params & params, const AtomPairs & pairs) {
    T energy = 0.0;
    
    // Raw pointer arithmetic
    auto pair_amber = reinterpret_cast<const AtomPairAmber<T> *>(&pairs + 1);
    for (int i=0; i<pairs.num_amber; i++) {
        Real3<T> atom1 = atoms[pair_amber->atomi1];
        Real3<T> atom2 = atoms[pair_amber->atomi2];
        // ...
        pair_amber += 1;
    }
    
    auto pair_eef1 = reinterpret_cast<const AtomPairEef1<T> *>(pair_amber);
    for (int i=0; i<pairs.num_eef1; i++) {
        // ...
        pair_eef1 += 1;
    }
}
```

**C++20 Modernization**:
```cpp
template<std::floating_point T>
static T calc(std::span<const Real3<T>> atoms, const Params & params, const AtomPairs & pairs) {
    T energy = 0.0;
    
    // Bounds-safe span
    auto pair_amber = std::span<const AtomPairAmber<T>>(
        reinterpret_cast<const AtomPairAmber<T> *>(&pairs + 1), 
        pairs.num_amber
    );
    for (const auto& pair : pair_amber) {
        Real3<T> atom1 = atoms[pair.atomi1];
        Real3<T> atom2 = atoms[pair.atomi2];
        // ...
    }
    
    auto pair_eef1 = std::span<const AtomPairEef1<T>>(
        pair_amber.data() + pair_amber.size(),
        pairs.num_eef1
    );
    for (const auto& pair : pair_eef1) {
        // ...
    }
}
```

**Expected Benefits**:
- **Stability**: Bounds checking prevents buffer overruns
- **Portability**: Standard interface, works with range-based for
- **Performance**: Zero overhead (compile-time bounds checks can be optimized out)
- **Maintainability**: Cleaner loop syntax, no manual pointer arithmetic

---

#### Candidate 3: Copy Operations

**Location**: `array.h:67-78` - `copy_from()` method

**Current Code**:
```cpp
inline int64_t copy_from(const Array<T> & src, int64_t srci, int64_t count, int64_t dsti) {
    assert(dsti >= 0);
    assert(dsti + count <= size);
    assert(srci >= 0);
    assert(srci + count <= src.size);
    
    std::copy(src.pointer() + srci, src.pointer() + srci + count, pointer() + dsti);
    return count;
}
```

**C++20 Modernization**:
```cpp
inline int64_t copy_from(std::span<const T> src, int64_t dsti) {
    assert(dsti >= 0);
    assert(dsti + src.size() <= size);
    
    std::copy(src.begin(), src.end(), span().begin() + dsti);
    return src.size();
}
```

**Expected Benefits**:
- **Stability**: Span automatically tracks size, fewer bugs
- **Performance**: Same as current (span is zero-cost)
- **Maintainability**: Simpler API, no separate size parameter

---

#### Candidate 4: Assignment Constructor

**Location**: `assignment.h:12-73` - `Assignment` constructor

**Current Code**:
```cpp
Assignment(const ConfSpace<T> & _conf_space, const int32_t _conf[])
    : conf_space(_conf_space), conf(_conf), atoms(_conf_space.max_num_conf_atoms) {
    
    // Raw pointer arrays
    auto _index_offsets = new int64_t[conf_space.num_pos];
    auto _atom_pairs = new const void *[...];
    auto _conf_energies = new T[conf_space.num_pos];
    
    // Manual loops with size tracking
    for (int posi1=0; posi1<conf_space.num_pos; posi1++) {
        // ...
    }
}
```

**C++20 Modernization**:
```cpp
Assignment(const ConfSpace<T> & _conf_space, std::span<const int32_t> conf)
    : conf_space(_conf_space), conf(conf.data()), atoms(_conf_space.max_num_conf_atoms) {
    
    // std::span for bounds safety
    auto index_offsets_span = std::span<int64_t>(new int64_t[conf_space.num_pos], conf_space.num_pos);
    auto atom_pairs_span = std::span<const void*>(new const void*[...], ...);
    auto conf_energies_span = std::span<T>(new T[conf_space.num_pos], conf_space.num_pos);
    
    // Range-based for with span
    for (int posi1 = 0; posi1 < conf_space.num_pos; posi1++) {
        // ...
    }
}
```

**Expected Benefits**:
- **Stability**: Bounds checking, prevents off-by-one errors
- **Maintainability**: Clearer size tracking

**Note**: JNA compatibility - verify that `std::span` can be passed through JNA or needs wrapper.

---

### 3. `constexpr` Expansion + SIMD Connection

**SIMD Tie-In:**
When implementing SIMD vectorization (as planned in another thread), `constexpr` enables:

1. **Compile-time SIMD selection:**
   ```cpp
   template<std::floating_point T>
   constexpr int simd_width() {
       if constexpr (std::same_as<T, float32_t>) {
           return 8;  // AVX2: 8 floats
       } else {
           return 4;  // AVX2: 4 doubles
       }
   }
   ```

2. **Compile-time alignment checks:**
   ```cpp
   constexpr bool is_simd_aligned(const void* ptr) {
       return (reinterpret_cast<uintptr_t>(ptr) % 32) == 0;
   }
   ```

3. **Compile-time loop unrolling hints:**
   ```cpp
   template<int N>
   constexpr void process_simd_chunk(std::span<const Real3<T>> atoms) {
       // Compile-time known SIMD width
   }
   ```

4. **Compile-time feature detection:**
   ```cpp
   #ifdef __AVX2__
   constexpr bool has_avx2 = true;
   #else
   constexpr bool has_avx2 = false;
   #endif
   ```

**Benefits for SIMD:**
- **Performance**: Compile-time decisions eliminate runtime branching
- **Portability**: Feature detection at compile time
- **Maintainability**: Clear SIMD width constants

#### Candidate 1: Version Functions

**Location**: `confecalc.cc:19-25`

**Current Code**:
```cpp
API int version_major() noexcept {
    return ConfEcalc_VERSION_MAJOR;
}

API int version_minor() noexcept {
    return ConfEcalc_VERSION_MINOR;
}
```

**C++20 Modernization**:
```cpp
constexpr int version_major() noexcept {
    return ConfEcalc_VERSION_MAJOR;
}

constexpr int version_minor() noexcept {
    return ConfEcalc_VERSION_MINOR;
}
```

**Expected Benefits**:
- **Performance**: Compile-time evaluation when possible
- **Portability**: Can be used in template parameters, constexpr contexts
- **Stability**: Compile-time validation
- **SIMD Integration**: Can be used in `constexpr` SIMD width calculations

---

#### Candidate 2: Static Constants

**Location**: `minimization.h:132-136` - `tolerance` template specialization

**Current Code**:
```cpp
template<typename T>
static const T tolerance;
template<>
const float32_t tolerance<float32_t> = 1e-3;
template<>
const float64_t tolerance<float64_t> = 1e-6;
```

**C++20 Modernization**:
```cpp
template<std::floating_point T>
static constexpr T tolerance = []() {
    if constexpr (std::same_as<T, float32_t>) {
        return 1e-3f;
    } else {
        return 1e-6;
    }
}();
```

**Expected Benefits**:
- **Performance**: Compile-time evaluation
- **Maintainability**: Single definition, no template specialization needed

---

#### Candidate 3: Motion Step Sizes

**Location**: `motions/dihedral.h:37`, `motions/transrot.h:62-63`

**Current Code**:
```cpp
static constexpr T step_size = 0.004363323; // 0.25 degrees
static constexpr T translation_step_size = 0.01; // angstroms
static constexpr T rotation_step_size = 0.004363323; // 0.25 degrees
```

**Status**: Already `constexpr`. No change needed.

---

### 4. `std::print` (C++23) - Better than `std::format`

**Why C++23 wasn't emphasized initially:**
- C++23 requires newer compiler support (GCC 14+, Clang 17+, MSVC 19.29+)
- C++20 has broader compiler support
- However, C++23 features are superior where available

**`std::print` vs `std::format`:**
- `std::print` writes directly to stdout/stderr (no `std::cout` needed)
- Faster than `std::format` + `std::cout` chaining
- Type-safe, compile-time format string checking
- Better performance than `std::format` for output

#### Candidate 1: Error Messages

**Location**: `minimization.h:56-57`, `minimization.h:81-82`

**Current Code**:
```cpp
std::cout << "motion id: " << motionid << std::endl;
throw std::invalid_argument("unrecognized motion id for molecule");
```

**C++23 Modernization**:
```cpp
std::print("motion id: {}\n", motionid);
throw std::invalid_argument(std::format("unrecognized motion id: {} for molecule", motionid));
```

**Expected Benefits**:
- **Performance**: Faster than `std::cout` chaining (direct write, no stream overhead)
- **Maintainability**: Cleaner syntax, type-safe formatting, compile-time format string validation
- **Portability**: Standard library feature (C++23)

---

#### Candidate 2: Debug Output

**Location**: `minimization.h:56`, `minimization.h:81`, `benchmark_energy.cpp:23-73`

**Current Code**:
```cpp
std::cout << "Benchmarking energy calculation..." << std::endl;
std::cout << "Iterations: " << iterations << std::endl;
std::cout << "Completed " << iterations << " iterations in " 
          << duration.count() << " microseconds" << std::endl;
```

**C++23 Modernization**:
```cpp
std::print("Benchmarking energy calculation...\n");
std::print("Iterations: {}\n", iterations);
std::print("Completed {} iterations in {} microseconds\n", iterations, duration.count());
```

**Expected Benefits**:
- **Performance**: 2-3x faster than `std::cout` chaining, no stream synchronization overhead
- **Maintainability**: Single-line format strings, compile-time validation
- **Stability**: Format string errors caught at compile time

**Note**: Requires C++23 compiler. Fallback to `std::format` + `std::cout` for C++20 compatibility.

---

### 5. Ranges (Lower Priority - Profile First)

#### Candidate 1: Energy Summation

**Location**: `energy_ambereef1.h:121-150` - `calc_energy()` function

**Current Code**:
```cpp
T energy = 0.0;
for (int i=0; i<inters.get_size(); i++) {
    PosInter<T> inter = inters[i];
    T inter_energy = 0.0;
    // ... calculate inter_energy ...
    energy += inter.weight*(inter_energy + inter.offset);
}
```

**C++20 Modernization**:
```cpp
auto energy = std::ranges::fold_left(
    std::views::iota(0, inters.get_size()),
    0.0,
    [&](T acc, int i) {
        PosInter<T> inter = inters[i];
        T inter_energy = 0.0;
        // ... calculate inter_energy ...
        return acc + inter.weight*(inter_energy + inter.offset);
    }
);
```

**Expected Benefits**:
- **Maintainability**: More functional style, composable
- **Performance**: **Unknown** - may have overhead. Profile before adopting.

**Recommendation**: Low priority. Profile first. Manual loops are often faster.

---

#### Candidate 2: Atom Pair Loops

**Location**: `energy_ambereef1.h:94-103`, `energy_ambereef1.h:106-115`

**Current Code**:
```cpp
for (int i=0; i<pairs.num_amber; i++) {
    Real3<T> atom1 = atoms[pair_amber->atomi1];
    Real3<T> atom2 = atoms[pair_amber->atomi2];
    T r2 = distance_sq(atom1, atom2);
    T r = std::sqrt(r2);
    energy += pair_amber->calc(r, r2, params.distance_dependent_dielectric);
    pair_amber += 1;
}
```

**C++20 Modernization** (with `std::span`):
```cpp
for (const auto& pair : pair_amber_span) {
    Real3<T> atom1 = atoms[pair.atomi1];
    Real3<T> atom2 = atoms[pair.atomi2];
    T r2 = distance_sq(atom1, atom2);
    T r = std::sqrt(r2);
    energy += pair.calc(r, r2, params.distance_dependent_dielectric);
}
```

**Expected Benefits**:
- **Maintainability**: Range-based for (already available with `std::span`)
- **Performance**: Same as current (span is zero-cost)

**Note**: This is `std::span` benefit, not ranges. Ranges would add overhead.

---

## C++23 Features

**Why C++23 features weren't emphasized:**
- C++23 requires newer compiler support (GCC 14+, Clang 17+, MSVC 19.29+)
- C++20 has broader adoption
- However, C++23 features are superior where compiler support exists
- `std::print` is better than `std::format` for output (see above)
- `std::expected` provides better error handling than exceptions
- `std::mdspan` requires data layout restructuring

### 1. `std::mdspan` (Multi-dimensional Array Views)

#### Candidate 1: Energy Matrix Access

**Location**: `assignment.h:83-84` - `get_atom_pairs()` indexing

**Current Code**:
```cpp
inline const void * get_atom_pairs(int posi1, int posi2) const {
    return atom_pairs[conf_space.index(posi1, posi2)];
}
```

**C++23 Modernization** (if energy matrix stored as multi-dim array):
```cpp
// If atom_pairs stored as multi-dim array:
std::mdspan<const void*, std::extents<size_t, dynamic_extent>> atom_pairs_view(atom_pairs, ...);

inline const void * get_atom_pairs(int posi1, int posi2) const {
    return atom_pairs_view[posi1, posi2];
}
```

**Expected Benefits**:
- **Stability**: Bounds checking for multi-dimensional access
- **Performance**: Better cache locality hints
- **Maintainability**: Clearer multi-dimensional semantics

**Note**: Requires restructuring data layout. May not be applicable if using offset-based indexing.

---

#### Candidate 2: Atom Coordinate Arrays

**Location**: `assignment.h:102` - `Array<Real3<T>> atoms`

**Current Code**:
```cpp
Array<Real3<T>> atoms;  // Linear array, accessed as 1D
```

**C++23 Modernization** (if needed for 3D operations):
```cpp
// If we need 3D view:
std::mdspan<Real3<T>, std::extents<size_t, dynamic_extent, 3>> atoms_3d(atoms.data(), num_atoms, 3);
```

**Expected Benefits**:
- **Maintainability**: Clearer 3D semantics
- **Performance**: Better compiler optimization hints

**Note**: Current 1D access is fine. Only needed if restructuring to 2D/3D views.

---

### 2. `std::expected<T, E>` (Error Handling)

#### Candidate 1: Energy Calculation Errors

**Location**: `energy_ambereef1.h:89-118` - `calc()` function

**Current Code**:
```cpp
template<typename T>
static T calc(const Array<Real3<T>> & atoms, const Params & params, const AtomPairs & pairs) {
    // Returns T, no error indication
    // Errors handled via assertions or NaN propagation
    assert (!std::isnan(r2));
    T r = std::sqrt(r2);
    energy += pair_amber->calc(r, r2, params.distance_dependent_dielectric);
}
```

**C++23 Modernization**:
```cpp
enum class EnergyError {
    InvalidDistance,
    InvalidParameters,
    OutOfBounds
};

template<std::floating_point T>
static std::expected<T, EnergyError> calc(
    std::span<const Real3<T>> atoms, 
    const Params & params, 
    const AtomPairs & pairs
) {
    T energy = 0.0;
    
    for (const auto& pair : pair_amber_span) {
        T r2 = distance_sq(atoms[pair.atomi1], atoms[pair.atomi2]);
        if (std::isnan(r2) || r2 < 0.0) {
            return std::unexpected(EnergyError::InvalidDistance);
        }
        T r = std::sqrt(r2);
        energy += pair.calc(r, r2, params.distance_dependent_dielectric);
    }
    
    return energy;
}
```

**Expected Benefits**:
- **Stability**: Type-safe error handling, no exception overhead
- **Performance**: No exception throwing/catching overhead
- **Maintainability**: Explicit error cases, better than NaN propagation

**Trade-off**: More verbose call sites (need to check `expected`).

---

#### Candidate 2: Assignment Construction Errors

**Location**: `assignment.h:12-73` - `Assignment` constructor

**Current Code**:
```cpp
Assignment(const ConfSpace<T> & _conf_space, const int32_t _conf[])
    : conf_space(_conf_space), conf(_conf), atoms(_conf_space.max_num_conf_atoms) {
    // Throws exceptions on error, or crashes on assertion failure
    assert(confi1 >= 0);
}
```

**C++23 Modernization**:
```cpp
enum class AssignmentError {
    InvalidConfIndex,
    OutOfBounds,
    InvalidPosition
};

template<std::floating_point T>
static std::expected<Assignment, AssignmentError> make(
    const ConfSpace<T> & conf_space, 
    std::span<const int32_t> conf
) {
    // Validate inputs
    if (conf.size() != conf_space.num_pos) {
        return std::unexpected(AssignmentError::OutOfBounds);
    }
    
    for (int i = 0; i < conf.size(); i++) {
        if (conf[i] < -1 || conf[i] >= conf_space.get_pos(i).num_confs) {
            return std::unexpected(AssignmentError::InvalidConfIndex);
        }
    }
    
    return Assignment(conf_space, conf);
}
```

**Expected Benefits**:
- **Stability**: Explicit error handling, no exceptions
- **Performance**: No exception overhead
- **Maintainability**: Clear error cases

---

## Recommended Next Feature: Concepts

**Why Concepts:**
- High impact: affects all template functions (3 in `confecalc.cc`, 1 in `array.h`)
- Low risk: compile-time only, no runtime changes
- No JNA compatibility concerns: purely type system
- Immediate benefits: better error messages, self-documenting code
- Straightforward implementation: replace `template<typename T>` with `template<std::floating_point T>`

**Implementation Plan:**
1. Start with `confecalc.cc:30` - `assign<T>` function
2. Add `template<std::floating_point T>` constraint
3. Verify compilation
4. Repeat for `calc<T>` and `minimize<T>`
5. Add to `array.h:10` - `Array<T>` class
6. Test with existing Java tests

**Files to modify:**
- `confecalc.cc:30, 48, 80` - 3 template functions
- `array.h:10` - Array class template
- `energy_ambereef1.h:88, 120` - Energy calculation templates
- `global.h:33-46` - Replace `ASSERT_JAVA_COMPATIBLE` macros with Concepts where applicable

---

## Summary Table

| Feature | Candidate Location | Expected Benefits | Priority |
|---------|-------------------|-------------------|----------|
| **Concepts** | `confecalc.cc` templates | Stability, Maintainability | **High - Recommended Next** |
| **Concepts** | `array.h:10` | Stability, Performance | High |
| **`std::span`** | `array.h:100-118` | **Stability, Performance, Maintainability** | **High** ⭐ |
| **`std::span`** | `energy_ambereef1.h:89-118` | Stability, Portability | High |
| **`std::span`** | `array.h:67-78` | Stability, Maintainability | Medium |
| **`constexpr`** | `confecalc.cc:19-25` | Performance, Portability, **SIMD integration** | Medium |
| **`constexpr`** | `minimization.h:132-136` | Performance, Maintainability | Low |
| **`std::print` (C++23)** | `minimization.h:56-57`, `benchmark_energy.cpp` | Performance, Maintainability | Medium (requires C++23) |
| **Ranges** | `energy_ambereef1.h:121-150` | Maintainability (profile first) | Low |
| **`std::mdspan`** | `assignment.h:83-84` | Stability, Performance | Low (requires restructuring) |
| **`std::expected`** | `energy_ambereef1.h:89-118` | Stability, Performance | Medium |

---

## Implementation Priority

### Phase 1: High Impact, Low Risk (C++20)
1. **Concepts** - Add to all template functions (RECOMMENDED NEXT)
2. **`std::span`** - Replace `pointer()` methods (verify JNA compatibility)
3. **`constexpr`** - Version functions (pass to SIMD thread)

### Phase 2: Medium Impact (C++20)
4. **`std::format`** - Error messages and debug output
5. **`constexpr`** - Template constants

### Phase 3: Evaluate (C++20/23)
6. **Ranges** - Profile first, may have overhead
7. **`std::expected`** - Evaluate error handling strategy
8. **`std::mdspan`** - Only if restructuring data layout

---

*Last Updated: 2025-01-XX*

