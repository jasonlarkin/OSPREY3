# OSPREY-Specific AVX-512 Issue

## Finding

Simple AVX-512 test program **DOES** generate AVX-512 instructions in WSL:
- `zmm: 2` registers found
- `vbroadcastsd %zmm0` instruction present
- Program runs successfully

OSPREY library **DOES NOT** generate AVX-512:
- Binary contains `ymm` registers (AVX2)
- No `zmm` registers found
- Same compiler, same WSL environment

## Possible Causes

### 1. CMake Target Architecture

OSPREY's CMake may set a specific target architecture that doesn't include AVX-512:

```cmake
# Check for:
set(CMAKE_SYSTEM_PROCESSOR ...)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=...")
```

**Investigation:**
```bash
cd src/main/cc/ConfEcalc
grep -E "march|mtune|target" CMakeLists.txt build/CMakeCache.txt
```

### 2. Compiler Optimization Level

Higher optimization may cause compiler to prefer AVX2 over AVX-512:

```cmake
# Check optimization level
grep "CMAKE_CXX_FLAGS_RELEASE\|O2\|O3" build/CMakeCache.txt
```

### 3. Header Include Order

The `#ifdef USE_AVX512` may not be evaluated correctly in OSPREY's build:

```cpp
// energy_ambereef1.h includes energy_ambereef1_simd.h
// But USE_AVX512 might not be defined at include time
```

**Check:**
```bash
# Verify preprocessor state
g++ -E -DUSE_AVX512 -DUSE_SIMD -I. energy_ambereef1.h | grep -E "i \+ 7|i \+ 3"
```

### 4. Function Inlining

The `calc_simd` function may be inlined and optimized away:

```cpp
static inline double calc_simd(...)  // inline may cause issues
```

**Solution:** Try removing `inline` or using `__attribute__((noinline))`

### 5. Template Specialization

The template system may be selecting the wrong code path:

```cpp
template<typename T>
static T calc(...) {
#ifdef USE_SIMD
    if constexpr (std::is_same_v<T, double>) {
        return calc_simd(...);  // May not be called
    }
#endif
    // Scalar version
}
```

## Investigation Steps

### Step 1: Check CMake Flags

```bash
cd src/main/cc/ConfEcalc
cat build/CMakeFiles/ConfEcalc.dir/flags.make | grep -E "march|mtune|target|avx"
```

### Step 2: Check Preprocessor Output

```bash
cd src/main/cc/ConfEcalc
g++ -E -DUSE_AVX512 -DUSE_SIMD \
    -I. -Ibuild \
    energy_ambereef1.h \
    | grep -A 5 "i + 7"
```

### Step 3: Check Object File

```bash
cd src/main/cc/ConfEcalc/build
objdump -d CMakeFiles/ConfEcalc.dir/confecalc.cc.o | grep -E "zmm|ymm" | head -10
```

### Step 4: Force AVX-512 in Template

Try explicitly calling `calc_simd` without the template wrapper:

```cpp
// In energy_ambereef1.h
template<typename T>
static T calc(...) {
#ifdef USE_AVX512
    if constexpr (std::is_same_v<T, double>) {
        return osprey::ambereef1::calc_simd(...);  // Explicit namespace
    }
#endif
    // ...
}
```

### Step 5: Check Linker Optimization

Linker may be optimizing away unused code:

```bash
# Check if calc_simd symbol exists
nm src/main/resources/linux-x86-64/libConfEcalc.so | grep calc_simd
```

## Resolution

**Root Cause:** Missing forward declaration of `calc_simd` function. The template was calling `calc_simd` before it was declared, causing the compiler to fall back to the scalar implementation.

**Solution Applied:**
1. Added forward declaration of `calc_simd` after `Params` and `AtomPairs` structs are defined
2. Moved `#include "energy_ambereef1_simd.h"` inside the namespace (before namespace closes)
3. Removed namespace declaration from `energy_ambereef1_simd.h` since it's included inside another header's namespace
4. Removed header guard from `energy_ambereef1_simd.h` to avoid conflicts

**Result:** Library now contains 140 AVX-512 instructions (`zmm` registers) vs 36 AVX2 instructions (`ymm` registers).

