# ConfEcalc Benchmark Results

## Baseline (Before SIMD Optimization)

**Test:** `edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64`

**Configuration:**
- 15 conformations
- 7 positions per conformation
- Calls `calc_amber_eef1_f64` (C++ native)
- Precision: Float64

**Results (First run - includes compilation):**
- Real time: 7m13.910s (433.91 seconds)
- User time: 0m20.504s
- Sys time: 0m5.549s
- Note: Includes Gradle compilation and daemon startup

**Results (Second run - actual test execution):**
- Real time: 0m19.885s (19.885 seconds)
- User time: 0m2.081s
- Sys time: 0m0.712s
- Status: PASSED
- Date: Baseline measurement

**Breakdown:**
- Total execution: 19.885 seconds
- Average per conformation: ~1.33 seconds
- Note: Includes JNA overhead, Java test framework, and C++ energy calculations
- The first run (7m13s) included compilation overhead; second run is the actual baseline

---

## SIMD Optimized (After Implementation)

**Test:** Same as baseline

**Configuration:**
- SIMD enabled: `ENABLE_SIMD=ON` in CMake
- AVX2 intrinsics: Processing 4 atom pairs simultaneously
- Compiler flags: `-mavx2 -mfma`

**Results (First run - includes full rebuild):**
- Real time: 17m9.999s (includes `--rerun-tasks` full rebuild)
- User time: 0m7.546s
- Sys time: 0m2.192s
- Status: PASSED
- Date: SIMD implementation complete

**Results (Clean run - test execution only):**
- Real time: 0m19.820s (19.820 seconds)
- User time: 0m3.082s
- Sys time: 0m0.449s
- Status: PASSED
- Date: Clean benchmark run

**Speedup:**
- 1.003x faster than baseline (19.885s → 19.820s)
- **Note:** Minimal speedup observed. Possible reasons:
  - SIMD overhead (gather/scatter operations for non-contiguous atom access)
  - Memory bandwidth bottleneck (loading atom coordinates)
  - Small number of pairs per iteration (SIMD overhead dominates)
  - JNA/Java overhead masks C++ improvements

**CPU Capabilities (11th Gen Intel Core i3-1115G4):**
- AVX2: Supported (256-bit, 4 doubles) - Currently used
- AVX-512: Supported (512-bit, 8 doubles) - Potential future optimization
- FMA: Supported (Fused Multiply-Add) - Enabled in build

---

## Notes

- Real time includes all overhead (JNA, Java framework, etc.)
- User time is actual CPU time used
- For accurate comparison, run multiple times and average
- Verify results match baseline (within numerical precision)

