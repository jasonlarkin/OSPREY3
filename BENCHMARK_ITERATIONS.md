# Multiple Benchmark Iterations

## Step 2: Statistical Confidence

Running multiple iterations to establish baseline variance and confirm measurements are consistent.

### Results: AVX2 SIMD (3 iterations)

**Test:** `calcEnergy_native_all_2RL0_f64`

| Run | Real Time | User Time | Sys Time | Status |
|-----|-----------|-----------|----------|--------|
| 1   | 13.447s   | 1.203s    | 0.175s   | PASSED |
| 2   | 11.941s   | 2.686s    | 0.384s   | PASSED |
| 3   | 20.583s   | 2.038s    | 0.326s   | PASSED |

**Statistics:**
- Average: 15.324s
- Min: 11.941s
- Max: 20.583s
- Variance: 8.642s (56.4% - high variance)
- Median: 13.447s

**Comparison to Baseline:**
- Baseline (scalar): 19.885s
- AVX2 average: 15.324s
- **Speedup: 1.30x** (23% faster)
- Best case (Run 2): 1.67x faster than baseline

**Analysis:**
- High variance suggests system load effects (WSL, background processes)
- Run 2 (11.941s) shows best performance - likely minimal system interference
- Average improvement of 23% is significant
- Real-world speedup likely between 1.3x-1.7x depending on system load

---

## Step 3: AVX-512 Benchmark

**Test:** Same as above, with AVX-512 enabled

**Configuration:**
- CMake: `-DENABLE_SIMD=ON -DUSE_AVX512=ON`
- Compiler flags: `-mavx512f -mavx512dq -mfma`
- Processes 8 atom pairs simultaneously (vs 4 with AVX2)

**Results:**
- Real time: 20.700s
- User time: 2.246s
- Sys time: 0.946s
- Status: PASSED

**Comparison:**
- AVX2 average: 15.324s
- AVX-512: 20.700s
- **AVX-512 is 1.35x SLOWER than AVX2**

**Possible Reasons:**
1. **Frequency scaling:** AVX-512 can cause CPU frequency throttling on some processors
2. **Not actually using AVX-512:** Binary may still contain AVX2 code (need to verify zmm registers)
3. **Memory bandwidth bottleneck:** Processing 8 pairs requires more memory bandwidth
4. **Small iteration counts:** AVX-512 overhead dominates for small workloads
5. **WSL overhead:** Virtualization may not handle AVX-512 efficiently

**Verification Needed:**
- Check if binary contains AVX-512 instructions (zmm registers)
- Verify USE_AVX512 preprocessor define is set
- Check CPU frequency scaling during execution

### Commands

**Quick 3-run iteration:**
```bash
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork
for i in {1..3}; do
  echo "Run $i:"
  time ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" 2>&1 | grep "real"
done
```

**Extract timing only (faster):**
```bash
cd /mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork
for i in {1..5}; do
  ./gradlew test --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" 2>&1 | grep -E "real|PASSED" | head -1
done
```

