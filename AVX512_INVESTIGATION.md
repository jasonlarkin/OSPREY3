# AVX-512 Investigation and Findings

## Summary

Despite CPU support for AVX-512 and correct CMake configuration, the compiled binary contains AVX2 instructions instead of AVX-512. This document details the investigation and potential causes.

## CPU Capabilities

**CPU:** 11th Gen Intel Core i3-1115G4 @ 3.00GHz

**AVX-512 Support Confirmed:**
```
avx512f, avx512dq, avx512ifma, avx512cd, avx512bw, avx512vl, 
avx512vbmi, avx512_vbmi2, avx512_vnni, avx512_bitalg, avx512_vpopcntdq
```

The CPU **definitely supports AVX-512**.

## Build Configuration

**CMake Configuration:**
```bash
cmake -B build -DENABLE_SIMD=ON -DUSE_AVX512=ON
```

**Result:**
- CMake reports: "SIMD optimizations enabled (AVX-512)"
- Compiler flags: `-mavx512f -mavx512dq -mfma`
- Preprocessor defines: `-DUSE_AVX512 -DUSE_SIMD`

**Build Files Verification:**
```bash
CXX_DEFINES = -DConfEcalc_EXPORTS -DUSE_AVX512 -DUSE_SIMD
CXX_FLAGS = -pedantic-errors -mavx512f -mavx512dq -mfma -fPIC
```

## Code Verification

**Preprocessor Test:**
```bash
g++ -E -DUSE_AVX512 -DUSE_SIMD energy_ambereef1_simd.h | grep "i + 7"
# Result: for (; i + 7 < num_amber; i += 8) {  AVX-512 code path
```

**Source Code:**
- `#ifdef USE_AVX512` block contains 8-iteration loops (AVX-512)
- `#else` block contains 4-iteration loops (AVX2)

## Binary Analysis

**AVX-512 Instructions:**
```bash
objdump -d libConfEcalc.so | grep -E "zmm[0-9]|62 [0-9a-f]{2}"
# Result: 0 AVX-512 instructions found
```

**AVX2 Instructions:**
```bash
objdump -d libConfEcalc.so | grep "c5 fd" | wc -l
# Result: 128+ AVX2 instructions found
```

**Assembly Analysis:**
- Binary contains `ymm` registers (256-bit, AVX2)
- Binary contains `c5 fd` prefix (AVX2 encoding)
- **No `zmm` registers (512-bit, AVX-512)**
- **No `62` prefix (EVEX encoding for AVX-512)**

**Loop Pattern:**
- Binary shows `i + 3 < num_amber; i += 4` pattern (AVX2)
- **Not** `i + 7 < num_amber; i += 8` pattern (AVX-512)

## Potential Causes

### 1. WSL Virtualization Impact

**Hypothesis:** WSL2 may not fully expose AVX-512 capabilities to the guest OS, or the Windows host CPU frequency scaling affects AVX-512.

**Evidence:**
- WSL2 runs in a virtualized environment
- CPU flags show AVX-512 support, but execution may be limited
- Windows host may throttle CPU frequency when AVX-512 is used

**Test:**
```bash
# Check if AVX-512 actually works in WSL
cat /proc/cpuinfo | grep flags | grep avx512
# Shows flags, but doesn't guarantee execution capability

# Check CPU frequency scaling
cat /proc/cpuinfo | grep "cpu MHz"
```

**Known Issues:**
- Some Intel CPUs throttle frequency when AVX-512 is active
- WSL2 may not properly handle AVX-512 frequency scaling
- Virtualization overhead may prevent efficient AVX-512 execution

### 2. Compiler Optimization

**Hypothesis:** GCC may be optimizing AVX-512 code to AVX2 for performance reasons, or due to target architecture settings.

**Possible Reasons:**
- Compiler detects that AVX2 is more efficient for this workload
- Memory bandwidth bottleneck makes AVX-512 overhead not worth it
- Compiler optimizes gather/scatter operations to AVX2

**Investigation:**
```bash
# Check compiler version and optimization level
g++ --version
# Check what optimizations are being applied
g++ -Q --help=optimizers | grep avx
```

### 3. Header Include Order

**Hypothesis:** The `#ifdef USE_AVX512` may not be evaluated correctly due to include order or preprocessor state.

**Code Structure:**
```cpp
// energy_ambereef1.h
#ifdef USE_SIMD
#include "energy_ambereef1_simd.h"  // Included here
#endif

// energy_ambereef1_simd.h
#ifdef USE_AVX512
  // AVX-512 code
#else
  // AVX2 code
#endif
```

**Potential Issue:**
- If `USE_AVX512` is defined after `USE_SIMD`, the header might be included before the define is available
- CMake `add_definitions()` order might matter

### 4. Library Not Fully Rebuilt

**Hypothesis:** Object files or cached compilation artifacts may prevent full rebuild.

**Solution Attempted:**
```bash
rm -rf build
cmake -B build -DENABLE_SIMD=ON -DUSE_AVX512=ON
cmake --build build
```

**Result:** Still no AVX-512 instructions in binary.

## Why Can't Identify the Switch?

**Challenges:**
1. **Compiler internals:** The decision to use AVX2 vs AVX-512 may happen in GCC's internal optimization passes
2. **No verbose output:** GCC doesn't report why it chose AVX2 over AVX-512
3. **Optimization passes:** Multiple optimization passes may transform AVX-512 code to AVX2
4. **Target architecture:** Default target may not include AVX-512

**Debugging Options:**
```bash
# Enable verbose compilation
cmake --build build VERBOSE=1

# Check intermediate assembly
g++ -S -mavx512f -mavx512dq source.cc -o source.s
# Inspect source.s for AVX-512 instructions

# Use compiler explorer (godbolt.org) to test
# Compile with different optimization levels
```

## Alternative Testing Environments

### Option 1: GitHub Actions

**Advantages:**
- Different CPU architecture (may be AMD or different Intel)
- Native Linux (no WSL virtualization)
- Consistent build environment
- Can test multiple CPU types

**Setup:**
```yaml
# .github/workflows/avx512-test.yml
jobs:
  test-avx512:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      - name: Build with AVX-512
        run: |
          cd src/main/cc/ConfEcalc
          cmake -B build -DENABLE_SIMD=ON -DUSE_AVX512=ON
          cmake --build build
      - name: Verify AVX-512
        run: |
          objdump -d libConfEcalc.so | grep -E "zmm|62 [0-9a-f]{2}" | wc -l
```

**GitHub Actions CPU:**
- Typically AMD EPYC or Intel Xeon
- May have different AVX-512 support
- Native execution (no virtualization)

### Option 2: Google Colab

**Advantages:**
- Free GPU/CPU access
- Can request specific CPU types
- Native Linux environment
- Can install custom toolchains

**Setup:**
```python
# In Colab notebook
!apt-get update
!apt-get install -y build-essential cmake
!git clone <repo>
!cd osprey-fork/src/main/cc/ConfEcalc
!cmake -B build -DENABLE_SIMD=ON -DUSE_AVX512=ON
!cmake --build build
!objdump -d libConfEcalc.so | grep zmm | wc -l
```

**Colab CPU:**
- Varies by instance
- May have better AVX-512 support
- Can test on different architectures

### Option 3: Local Native Linux

**If available:**
- Dual boot Linux
- Native Linux VM (not WSL)
- Docker container with AVX-512 passthrough

## Recommendations

### Immediate Actions

1. Document current findings (this document)
2. **Test on GitHub Actions** - Different CPU may show different behavior
3. **Test on Google Colab** - Verify if WSL is the issue
4. **Check compiler verbose output** - See what GCC is actually doing

### Investigation Steps

1. **Compile with `-S` flag** to see assembly output:
   ```bash
   g++ -S -mavx512f -mavx512dq -DUSE_AVX512 source.cc
   # Check if .s file contains AVX-512 instructions
   ```

2. **Test minimal AVX-512 program:**
   ```cpp
   #include <immintrin.h>
   int main() {
       __m512d a = _mm512_set1_pd(1.0);
       __m512d b = _mm512_set1_pd(2.0);
       __m512d c = _mm512_add_pd(a, b);
       return 0;
   }
   ```
   Compile and check if AVX-512 instructions are generated.

3. **Check GCC optimization reports:**
   ```bash
   g++ -mavx512f -mavx512dq -ftree-vectorizer-verbose=2 source.cc
   ```

### Long-term Solutions

1. **Accept AVX2 performance** - 1.3x speedup is still significant
2. **Profile to find bottlenecks** - May reveal why AVX-512 isn't beneficial
3. **Optimize memory access** - Structure of Arrays (SoA) may enable AVX-512
4. **Test on native hardware** - Eliminate WSL as a variable

## Current Status

- CPU supports AVX-512
- CMake configuration correct
- Preprocessor defines correct
- Source code has AVX-512 path
- Binary contains AVX2 only
- Root cause unknown

## Minimal AVX-512 Test

**Test Program:**
```cpp
#include <immintrin.h>
__m512d a = _mm512_set1_pd(1.0);
__m512d b = _mm512_set1_pd(2.0);
__m512d c = _mm512_add_pd(a, b);
```

**Compilation:**
```bash
g++ -mavx512f -mavx512dq -O2 test_avx512.cpp -o test_avx512
```

**Result:** Compiles successfully, but binary analysis needed to verify AVX-512 instructions.

**Conclusion:** Compiler can compile AVX-512 code, but may optimize it away or convert to AVX2.

## Next Steps

1. Test minimal AVX-512 program to verify compiler capability
2. Set up GitHub Actions workflow to test on different CPU
3. Test on Google Colab to eliminate WSL variable
4. Check if compiler optimization level affects AVX-512 generation
5. Document results and decide on path forward

