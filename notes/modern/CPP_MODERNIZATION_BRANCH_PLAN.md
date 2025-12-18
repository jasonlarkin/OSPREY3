# C++ Modernization Branch Plan

## Overview

Modernizations independent of SIMD work that can be done on a new branch from `feature/simd-quick-wins` or `develop`.

## Recommended Modernizations (Priority Order)

### 1. **Concepts** (C++20) - HIGHEST PRIORITY

**Rationale:**
- Compile-time only, zero runtime impact
- No JNA compatibility concerns (type system only)
- High impact: affects 13+ template files
- Better error messages, self-documenting code
- Independent of SIMD (SIMD code uses explicit types, not templates)

**Files to modify:**
- `confecalc.cc:30, 48, 80` - 3 core template functions
- `array.h:10` - Array class template
- `energy_ambereef1.h:19, 54, 137, 175` - Energy templates
- `real3.h`, `rotation.h`, `motions/*.h`, `minimization.h`, `confspace.h`, `assignment.h`, `atoms.h`

**Implementation:**
1. Update `CMakeLists.txt`: `set(CMAKE_CXX_STANDARD 20)`
2. Replace `template<typename T>` with `template<std::floating_point T>` for floating-point templates
3. Replace with `template<std::trivially_copyable T>` for Array (verify JNA compatibility)
4. Add `#include <concepts>` where needed

**Expected benefits:**
- Compile-time type safety
- Better IDE support
- Clearer error messages
- Self-documenting constraints

---

### 2. **`constexpr`** (C++17/C++20) - **EXCLUDED: IN SIMD BRANCH**

**Status:** Being worked on in `feature/simd-quick-wins` branch alongside SIMD implementation.

**Rationale for exclusion:**
- SIMD work includes `constexpr` optimizations for compile-time SIMD selection
- Duplicating work would create merge conflicts
- SIMD branch handles `constexpr` in context of vectorization needs

**Do not implement on modernization branch.**

---

### 3. **`std::format`** (C++20) or **`std::print`** (C++23) - LOW PRIORITY

**Rationale:**
- Only 2 locations in production code (`minimization.h`)
- Test/benchmark output is fine as-is
- SIMD debug output uses `fprintf` (appropriate for stderr)

**Files to modify:**
- `minimization.h:56, 81` - Error messages only

**Current:**
```cpp
std::cout << "motion id: " << motionid << std::endl;
throw std::invalid_argument("unrecognized motion id for molecule");
```

**C++20 Modernization:**
```cpp
throw std::invalid_argument(std::format("unrecognized motion id: {} for molecule", motionid));
```

**C++23 Modernization (better):**
```cpp
std::print("motion id: {}\n", motionid);
throw std::invalid_argument(std::format("unrecognized motion id: {} for molecule", motionid));
```

**Expected benefits:**
- Cleaner syntax
- Type-safe formatting
- Better performance (C++23 `std::print`)

**Note:** Requires C++23 compiler for `std::print`, C++20 for `std::format`.

---

### 4. **`std::span`** (C++20) - MEDIUM PRIORITY, NEEDS VERIFICATION

**Rationale:**
- High impact: bounds-safe array access
- Performance: zero-cost abstraction
- Stability: compile-time bounds checking

**Blocking concern:**
- JNA compatibility: `Array::pointer()` returns raw pointers
- Need to verify JNA can handle `std::span` types
- May require keeping `pointer()` for JNA compatibility

**Files to modify:**
- `array.h:100-118` - `pointer()` methods (add `span()` methods)
- `energy_ambereef1.h:149-170` - Raw pointer loops (use span iteration)
- `energy_ambereef1_simd.h` - SIMD code (keep raw pointers for intrinsics)

**Implementation strategy:**
1. Add `span()` methods alongside `pointer()` (don't remove `pointer()` yet)
2. Update internal loops to use `std::span`
3. Verify JNA still works with `pointer()` method
4. Benchmark to ensure no performance regression
5. Gradually migrate call sites

**Expected benefits:**
- Bounds safety
- Cleaner iteration syntax
- Better optimization hints

**Defer if:** JNA compatibility is unclear or requires extensive testing.

---

## Implementation Order

1. **Phase 1: Concepts** (Low risk, high value)
   - Update CMakeLists.txt to C++20
   - Start with `confecalc.cc` (3 functions)
   - Verify compilation and Java tests pass
   - Expand to other template files

2. **Phase 2: `std::format`/`std::print`** (Low impact)
   - Only if time permits
   - Test error message formatting

3. **Phase 3: `std::span`** (After JNA verification)
   - Requires careful testing
   - Keep `pointer()` for JNA compatibility
   - Gradual migration

## Branch Strategy

**Recommended:** Create branch from `develop` (not from `feature/simd-quick-wins`)

**Branch name:** `feature/cpp20-modernization`

**Rationale:**
- SIMD work is separate concern
- Modernizations are independent
- Easier to merge both branches separately
- `develop` is the stable base

## Testing Requirements

1. **Compilation:**
   ```bash
   cd src/main/cc/ConfEcalc
   cmake -B build
   cmake --build build
   ```

2. **Java tests:**
   ```bash
   ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator"
   ```

3. **JNA compatibility:**
   - Verify library loads correctly
   - Verify function calls work
   - Verify data structures match expected layout

## Excluded from This Branch

- **`constexpr`** (being worked on in `feature/simd-quick-wins` alongside SIMD implementation)
- **SIMD-related changes** (already in `feature/simd-quick-wins`)
- **DeepCopy deserializer** (user working on this separately)
- **EPIC matrix** (user mentioned after DeepCopy)
- **Performance optimizations** (SIMD branch handles this)

## Risk Assessment

| Modernization | Risk | Impact | Recommendation |
|--------------|------|--------|----------------|
| Concepts | Low | High | **Do first** |
| `constexpr` | N/A | N/A | **EXCLUDED: In SIMD branch** |
| `std::format`/`std::print` | Low | Low | Optional |
| `std::span` | Medium | High | Verify JNA first |

## Success Criteria

- All modernizations compile with C++20
- All Java tests pass
- JNA compatibility maintained
- No performance regression
- Code is more maintainable

