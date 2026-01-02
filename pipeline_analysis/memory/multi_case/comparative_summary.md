# Comparative GC Analysis Across Test Cases

## Summary

| Test Case | GC Events | Young GC | Full GC | Avg Pause (ms) | Max Pause (ms) | GC Overhead (%) | GC Rate (events/s) |
|-----------|-----------|----------|---------|----------------|----------------|-----------------|-------------------|
| 1GUA11 | 36 | 24 | 0 | 15.14 | 46.75 | 0.47 | 0.308 |
| 2RL0 | 43 | 25 | 0 | 11.02 | 42.95 | 0.33 | 0.296 |
| 2RL0_no_wt | 45 | 27 | 0 | 9.12 | 24.22 | 0.53 | 0.581 |
| 2RL0_one_mutant | 45 | 25 | 0 | 9.97 | 29.46 | 0.58 | 0.578 |

## Observations

Compare GC patterns across different system sizes and configurations to identify:
- Which test cases show highest GC pressure
- Whether GC behavior scales with system size
