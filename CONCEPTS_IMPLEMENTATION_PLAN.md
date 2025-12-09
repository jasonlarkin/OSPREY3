# Concepts Implementation Plan

## Overview

Add C++20 Concepts to template functions in ConfEcalc to enforce type constraints at compile time.

## Benefits

- **Stability**: Compile-time type checking prevents wrong template instantiation
- **Maintainability**: Self-documenting code, better IDE support, clearer error messages
- **Performance**: Zero runtime overhead (compile-time only)
- **No JNA concerns**: Purely type system, no ABI changes

## Files to Modify

### 1. `confecalc.cc`

**Current (Line 30):**
```cpp
template<typename T>
static void assign(const ConfSpace<T> & conf_space, const int32_t conf[], Array<Real3<T>> & out_coords) {
```

**Modernized:**
```cpp
template<std::floating_point T>
static void assign(const ConfSpace<T> & conf_space, const int32_t conf[], Array<Real3<T>> & out_coords) {
```

**Current (Line 48):**
```cpp
template<typename T>
static T calc(const ConfSpace<T> & conf_space, const int32_t conf[], ...) {
```

**Modernized:**
```cpp
template<std::floating_point T>
static T calc(const ConfSpace<T> & conf_space, const int32_t conf[], ...) {
```

**Current (Line 80):**
```cpp
template<typename T>
static T minimize(const ConfSpace<T> & conf_space, const int32_t conf[], ...) {
```

**Modernized:**
```cpp
template<std::floating_point T>
static T minimize(const ConfSpace<T> & conf_space, const int32_t conf[], ...) {
```

### 2. `array.h`

**Current (Line 10):**
```cpp
template<typename T>
class Array {
```

**Modernized:**
```cpp
template<std::trivially_copyable T>
class Array {
```

**Note**: Verify this doesn't break JNA compatibility. `Array` must remain standard layout.

### 3. `energy_ambereef1.h`

**Current (Line 88):**
```cpp
template<typename T>
static T calc(const Array<Real3<T>> & atoms, const Params & params, const AtomPairs & pairs) {
```

**Modernized:**
```cpp
template<std::floating_point T>
static T calc(const Array<Real3<T>> & atoms, const Params & params, const AtomPairs & pairs) {
```

**Current (Line 120):**
```cpp
template<typename T>
T calc_energy(Assignment<T> & assignment, const Array<PosInter<T>> & inters) {
```

**Modernized:**
```cpp
template<std::floating_point T>
T calc_energy(Assignment<T> & assignment, const Array<PosInter<T>> & inters) {
```

### 4. Other Template Files

Check these files for `template<typename T>` that should be constrained:
- `rotation.h` - Line 8, 11, 14, 17, 33, 121
- `real3.h` - Line 8, 68, 77, 86, 95, 104, 114, 122
- `motions/transrot.h` - Line 8
- `motions/dihedral.h` - Line 8
- `motions.h` - Line 9
- `minimization.h` - Line 8, 27, 131, 139, 146, 355, 361, 364
- `confspace.h` - Line 10, 30, 268
- `atoms.h` - Line 8, 13
- `assignment.h` - Line 8

## Implementation Steps

1. **Verify C++20 support:**
   ```bash
   cd src/main/cc/ConfEcalc
   grep -r "CMAKE_CXX_STANDARD" CMakeLists.txt
   # Should show C++17, update to C++20
   ```

2. **Update CMakeLists.txt:**
   ```cmake
   set(CMAKE_CXX_STANDARD 20)
   ```

3. **Add Concepts include (if needed):**
   ```cpp
   #include <concepts>  // C++20 concepts
   ```

4. **Start with `confecalc.cc`:**
   - Replace `template<typename T>` with `template<std::floating_point T>` on lines 30, 48, 80
   - Compile and test

5. **Test compilation:**
   ```bash
   cd src/main/cc/ConfEcalc
   cmake -B build
   cmake --build build
   ```

6. **Run Java tests:**
   ```bash
   ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator"
   ```

7. **Continue with other files:**
   - `array.h` (verify JNA compatibility)
   - `energy_ambereef1.h`
   - Other template files as needed

## Verification

After implementation, verify:
- Code compiles with C++20
- Java tests pass (JNA compatibility maintained)
- Error messages are clearer when wrong types are used
- No performance regression

## Expected Compile Errors (Good!)

If someone tries to instantiate with wrong types:
```cpp
// This should now fail at compile time:
osprey::assign<int>(conf_space, conf, out_coords);  // Error: int doesn't satisfy std::floating_point
```

The error message should be clearer than before.

