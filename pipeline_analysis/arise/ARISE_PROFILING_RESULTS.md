# ARISE Profiling Results

## Summary Location

**Main Summary File:**
- `pipeline_analysis/arise/test_arise_10_matches/arise_profiling_summary.md`

**Detailed Results:**
- `arise_profile.prof` - cProfile binary (use `python3 -m pstats` to analyze)
- `arise_profile_top50_cumulative.txt` - Top functions by cumulative time
- `arise_profile_top50_self.txt` - Top functions by self time
- `arise_system_resources.txt` - System resource usage (if generated)
- `arise_io_analysis.txt` - I/O patterns
- `arise_memory_profile.txt` - Memory profiling (if memory_profiler available)

## Key Findings from Current Run

**Execution Time:** 23.2 seconds total
- Module imports: 21.7s (93% of time)
  - `osprey/prep.py`: 7.4s
  - JVM startup: 3.3s
  - VTK/pyvista: 5.3s
- Actual ARISE processing: < 2s

**Function Calls:** 902K total
- Most time in import/bootstrap overhead
- Actual computation is fast once loaded

**Note:** The summary file shows empty system resources because the script may have failed to write them. Check the individual files for details.

