# SIMD Runtime Dispatch Implementation Plan

## Current State (Problems)

### 1. **Compile-Time Only Selection**
- SIMD version is selected at **compile time** via `#ifdef USE_SIMD` and `#ifdef USE_AVX512`
- Only **ONE version** gets compiled (scalar, AVX2, OR AVX-512)
- The template uses `if constexpr` to call `calc_simd()` at compile time

### 2. **No Runtime CPU Detection**
- Build process checks **compiler support** (can the compiler generate AVX-512?)
- Does **NOT** check **CPU runtime support** (does the CPU running the binary support AVX-512?)
- Result: Binary compiled with AVX-512 flags will **crash** on CPUs without AVX-512 support

### 3. **GitHub Actions Failure Analysis**
- **CPU**: AMD EPYC 7763 (does NOT support AVX-512)
- **Build**: Succeeded (compiler supports AVX-512)
- **Runtime**: Would crash if AVX-512 code executed (illegal instruction)
- **Also**: Java compilation failed due to `RunAnalysis.java` (now fixed locally)

### 4. **No Fallback Mechanism**
- If AVX-512 isn't available, no automatic fallback to AVX2
- If AVX2 isn't available, no automatic fallback to scalar
- Requires separate builds for different CPU capabilities

## Required Solution: Runtime CPU Detection + Multi-Version Build

### Architecture Overview

```
┌─────────────────────────────────────────────────────────────┐
│  Build Time: Compile ALL versions (scalar, AVX2, AVX-512)  │
│  - Build with all SIMD flags: -mavx2 -mavx512f -mavx512dq  │
│  - All implementations included via function pointers       │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  Runtime: CPU Detection (first call)                        │
│  - Use cpuid or /proc/cpuinfo                               │
│  - Detect: AVX-512 → AVX2 → Scalar                          │
│  - Set function pointer to best available version           │
└─────────────────────────────────────────────────────────────┘
                           ↓
┌─────────────────────────────────────────────────────────────┐
│  Runtime: Dispatch to selected implementation               │
│  - calc() → dispatch to calc_scalar/calc_avx2/calc_avx512   │
└─────────────────────────────────────────────────────────────┘
```

### Implementation Steps

#### Step 1: CPU Detection Utility
Create `cpu_detect.h` with runtime CPU capability detection:
- Detect AVX-512 support (`__cpuid_count` or `/proc/cpuinfo`)
- Detect AVX2 support
- Return capability flags

#### Step 2: Multiple SIMD Implementations
Compile **all** versions simultaneously:
- `calc_scalar()` - Always available
- `calc_avx2()` - Compiled with `-mavx2` but only called if CPU supports it
- `calc_avx512()` - Compiled with `-mavx512f -mavx512dq` but only called if CPU supports it

#### Step 3: Runtime Dispatch
- First call to `calc()`: Detect CPU capabilities and initialize function pointer
- Subsequent calls: Use cached function pointer (fast dispatch)

#### Step 4: Build Configuration Changes
```cmake
# Always enable all SIMD flags if compiler supports them
# CPU detection handles which version to use at runtime
if(ENABLE_SIMD)
    check_cxx_compiler_flag("-mavx512f" HAS_AVX512)
    check_cxx_compiler_flag("-mavx2" HAS_AVX2)
    
    if(HAS_AVX512)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx512f -mavx512dq")
    endif()
    if(HAS_AVX2)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -mavx2 -mfma")
    endif()
    
    # Always define USE_SIMD to enable all implementations
    add_definitions(-DUSE_SIMD)
endif()
```

## Current Build Process Flow

### Local Build (WSL - CPU supports AVX-512)
1. CMake: `-DENABLE_SIMD=ON -DUSE_AVX512=ON`
2. Compiler flags: `-mavx512f -mavx512dq -mfma`
3. Preprocessor: `-DUSE_SIMD -DUSE_AVX512`
4. Result: Only AVX-512 version compiled
5. Runtime: Works on local CPU (supports AVX-512)
6. Runtime: Would crash on GitHub Actions CPU (doesn't support AVX-512)

### GitHub Actions Build
1. CMake: Same flags
2. Compiler flags: Same (compiler supports AVX-512)
3. Preprocessor: Same
4. Result: Only AVX-512 version compiled
5. Runtime: Would crash (CPU doesn't support AVX-512)

## Benchmarking Questions

### Q: Are both original and SIMD versions built and benchmarked?
**A: NO** - Currently only ONE version is built at a time. To benchmark:
- Need separate builds: one with `ENABLE_SIMD=OFF` and one with `ENABLE_SIMD=ON`
- Or need runtime dispatch to allow A/B comparison

### Q: How is SIMD triggered during build?
**A: Compile-time** via CMake options:
- `ENABLE_SIMD=ON` → defines `USE_SIMD`, includes SIMD code
- `USE_AVX512=ON` → defines `USE_AVX512`, selects AVX-512 version
- Template `if constexpr` selects SIMD path at compile time

### Q: What if AVX-512 isn't available?
**A: Current behavior:**
- If compiler doesn't support it → Falls back to AVX2 (CMake check)
- If CPU doesn't support it → **CRASH** (no runtime check)

**Required behavior:**
- Runtime detection → Automatic fallback to best available version

## Next Steps

1. **Immediate**: Fix GitHub Actions (remove `RunAnalysis.java` - already done locally)
2. **Short-term**: Implement runtime CPU detection and dispatch
3. **Medium-term**: Build all versions simultaneously for comparison
4. **Long-term**: Add benchmarking infrastructure to compare versions

## Implementation Priority

1. **High**: Runtime CPU detection (prevent crashes on unsupported CPUs)
2. **High**: Multiple implementation compilation (enable runtime selection)
3. **Medium**: Runtime dispatch mechanism (function pointers)
4. **Low**: Benchmarking framework (A/B testing between versions)

