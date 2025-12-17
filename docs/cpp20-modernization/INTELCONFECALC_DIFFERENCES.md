# IntelConfEcalc vs ConfEcalc Differences

## Summary

IntelConfEcalc is an older/incomplete version of ConfEcalc, not just a compiler flag variant. It's missing features and uses different implementations.

## Differences Found (Compared to Original Main Branch)

### 1. Version Constants
- **ConfEcalc**: Uses `ConfEcalc_VERSION_MAJOR` / `ConfEcalc_VERSION_MINOR`
- **IntelConfEcalc**: Uses `IntelConfEcalc_VERSION_MAJOR` / `IntelConfEcalc_VERSION_MINOR`
- **Impact**: Minimal - just naming for library identification

### 2. Missing AutoArray Class
- **ConfEcalc**: Has `AutoArray<T>` class (size-tracking array backed by Array)
- **IntelConfEcalc**: Missing `AutoArray` entirely
- **Impact**: Significant - IntelConfEcalc uses raw arrays instead

### 3. Different DOF Array Implementation
- **ConfEcalc**: Uses `AutoArray<Dof<T> *>` for DOF management
  ```cpp
  dofs = new AutoArray<Dof<T> *>(assignment.conf_space.max_num_dofs);
  ```
- **IntelConfEcalc**: Uses raw array with manual size tracking
  ```cpp
  dofs = new Dof<T> *[assignment.conf_space.max_num_dofs];
  size = 0;
  ```
- **Impact**: More error-prone, manual memory management

### 4. Minor Syntax Differences
- **ConfEcalc**: Uses `auto` keyword in switch statements
  ```cpp
  switch (auto motionid = assignment.conf_space.get_molecule_motion_id(motioni)) {
  ```
- **IntelConfEcalc**: Explicit type
  ```cpp
  switch (assignment.conf_space.get_molecule_motion_id(motioni)) {
  ```
- **Impact**: Minimal - style difference

### 5. Compiler Configuration
- **ConfEcalc**: Uses standard GCC/Clang compiler
- **IntelConfEcalc**: Requires Intel compiler (`icc`) with Intel-specific flags:
  - `-static-intel`
  - `-qopt-report=5`
  - `-qopt-report-phase=vec`
- **Impact**: Different build system, Intel-specific optimizations

## Files with Differences

Based on comparison with original main branch:
- `confecalc.cc` - Version constants only
- `array.h` - Missing AutoArray class
- `minimization.h` - Different DOF array implementation
- `real3.h` - No differences
- `energy_ambereef1.h` - No differences

## Conclusion

IntelConfEcalc appears to be:
1. An older snapshot of the code (before AutoArray was added)
2. A variant optimized for Intel compiler
3. Missing modern features (AutoArray abstraction)
4. Using more manual/error-prone memory management

## Recommendation

If IntelConfEcalc is still needed:
- Update to match ConfEcalc features (add AutoArray)
- Apply C++20 modernizations (Concepts, [[nodiscard]], noexcept)
- Consolidate to shared source with compiler-specific build configs

If IntelConfEcalc is not actively used:
- Remove to reduce maintenance burden
- Or document as deprecated/legacy

