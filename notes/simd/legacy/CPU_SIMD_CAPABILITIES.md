# CPU SIMD Capabilities

## System Information

**CPU:** 11th Gen Intel Core i3-1115G4 @ 3.00GHz

## Supported SIMD Instruction Sets

### SSE (128-bit)
- **SSE, SSE2, SSE3, SSSE3, SSE4.1, SSE4.2**: Supported
- Processes 2 `double` values simultaneously
- Legacy, but widely compatible

### AVX (256-bit)
- **AVX**: Supported
- Processes 4 `float` values or 2 `double` values simultaneously
- Foundation for AVX2

### AVX2 (256-bit) - Currently Used
- **AVX2**: Supported
- Processes 4 `double` values simultaneously
- **FMA (Fused Multiply-Add)**: Supported
- Current implementation uses AVX2 for energy calculations

### AVX-512 (512-bit) - Available for Future Optimization
- **AVX-512 Foundation (AVX512F)**: Supported
- **AVX-512 Doubleword and Quadword (AVX512DQ)**: Supported
- **AVX-512 Conflict Detection (AVX512CD)**: Supported
- **AVX-512 Byte and Word (AVX512BW)**: Supported
- **AVX-512 Vector Length (AVX512VL)**: Supported
- **AVX-512 Integer Fused Multiply-Add (AVX512IFMA)**: Supported
- **AVX-512 Vector Byte Manipulation (AVX512VBMI)**: Supported
- **AVX-512 Vector Neural Network (AVX512_VNNI)**: Supported
- **AVX-512 Bit Algorithms (AVX512_BITALG)**: Supported
- **AVX-512 Vector Population Count (AVX512_VPOPCNTDQ)**: Supported
- Processes 8 `double` values simultaneously
- **Potential 2x speedup** over AVX2 for suitable workloads

## Current Implementation

- **Using:** AVX2 (4 doubles at a time)
- **Compiler flags:** `-mavx2 -mfma`
- **Performance:** Minimal speedup observed (1.003x)
  - Possible reasons:
    - Non-contiguous memory access (gather/scatter overhead)
    - Memory bandwidth bottleneck
    - Small iteration counts (SIMD overhead dominates)
    - JNA/Java overhead masks improvements

## Future Optimization Opportunities

1. **AVX-512 Implementation**
   - Process 8 atom pairs simultaneously (vs 4 with AVX2)
   - Requires: `-mavx512f -mavx512dq` compiler flags
   - Potential 2x speedup for compute-bound sections

2. **Memory Layout Optimization**
   - Structure of Arrays (SoA) instead of Array of Structures (AoS)
   - Enables efficient vectorized loads without gather/scatter

3. **Larger Batch Processing**
   - Process more pairs per iteration to amortize SIMD overhead
   - Better cache utilization

## Verification Commands

```bash
# Check CPU flags
cat /proc/cpuinfo | grep flags

# Check if AVX2 is available
grep -o 'avx2' /proc/cpuinfo | head -1

# Check if AVX-512 is available
grep -o 'avx512' /proc/cpuinfo | head -1

# Verify compiler support
gcc -march=native -Q --help=target | grep avx
```

