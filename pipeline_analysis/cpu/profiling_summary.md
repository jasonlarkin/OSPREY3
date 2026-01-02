# Profiling Results Summary

## Test Cases Analyzed

### alloc_profile

**Allocation Profile:**
- Total samples: 464
- Application code: 0.0%
- Infrastructure: 0.0%

### cpu_profile

**CPU Profile:**
- Total samples: 363
- Application code: 0.0%
- Infrastructure: 0.0%

### test1GUA11

**CPU Profile:**
- Total samples: 771
- Application code: 0.0%
- Infrastructure: 0.0%
- Hotspots:
  - native_energy: 2.08%

**Allocation Profile:**
- Total samples: 626
- Application code: 0.0%
- Infrastructure: 0.0%
- Hotspots:
  - native_energy: 1.76%

### test2RL0

**CPU Profile:**
- Total samples: 900
- Application code: 0.0%
- Infrastructure: 0.0%
- Hotspots:
  - native_energy: 1.22%

**Allocation Profile:**
- Total samples: 1,741
- Application code: 0.0%
- Infrastructure: 0.0%
- Hotspots:
  - native_energy: 4.31%

### test2RL0OnlyOneMutant

**CPU Profile:**
- Total samples: 363
- Application code: 0.0%
- Infrastructure: 0.0%
- Hotspots:
  - native_energy: 5.51%

**Allocation Profile:**
- Total samples: 464
- Application code: 0.0%
- Infrastructure: 0.0%
- Hotspots:
  - native_energy: 3.02%

## Cross-Case Analysis

### CPU Processing Patterns (Across All Cases)

| Category | Total Samples | Cases |
|----------|---------------|-------|
| native_energy | 47 | test2RL0OnlyOneMutant, test2RL0, test1GUA11 |

### Memory Allocation Patterns (Across All Cases)

| Category | Total Samples | Cases |
|----------|---------------|-------|
| native_energy | 100 | test2RL0OnlyOneMutant, test2RL0, test1GUA11 |

## Workload Characteristics

### Overall Statistics

- Total CPU samples: 2,397
- Total allocation samples: 3,295
- Test cases: 5

### Key Findings

**Warning:** Profiles show mostly infrastructure (Gradle/JVM) overhead.
This suggests:
- Test execution may be too fast to capture application code
- Profiling may be capturing setup/teardown rather than computation
- Application code may be in native/C++ components not visible to JVM profiler

### Processing Requirements

**CPU-Intensive Operations:**
- native_energy: 47 samples across 3 case(s)

**Memory-Intensive Operations:**
- native_energy: 100 samples across 3 case(s)

### Recommendations

Based on profiling data:

- **Native energy calculations** are CPU-intensive - consider SIMD/GPU acceleration
