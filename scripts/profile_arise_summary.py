#!/usr/bin/env python3
"""Generate summary report for ARISE profiling."""
import sys
from pathlib import Path

output_base = Path(sys.argv[1])
summary_file = output_base / "arise_profiling_summary.md"

# Read results
system_resources = {}
if (output_base / "arise_system_resources.txt").exists():
    with open(output_base / "arise_system_resources.txt") as f:
        for line in f:
            if ':' in line:
                key, value = line.split(':', 1)
                system_resources[key.strip()] = value.strip()

io_stats = {}
if (output_base / "arise_io_analysis.txt").exists():
    with open(output_base / "arise_io_analysis.txt") as f:
        for line in f:
            if ':' in line:
                key, value = line.split(':', 1)
                io_stats[key.strip()] = value.strip()

# Generate summary
with open(summary_file, 'w') as f:
    f.write("# ARISE Profiling Summary\n\n")
    f.write("## System Resources\n\n")
    for key, value in system_resources.items():
        f.write(f"- **{key}**: {value}\n")
    f.write("\n## I/O Statistics\n\n")
    for key, value in io_stats.items():
        f.write(f"- **{key}**: {value}\n")
    f.write("\n## Files Generated\n\n")
    f.write("- `arise_profile.prof` - cProfile binary data\n")
    f.write("- `arise_profile_top50_cumulative.txt` - Top functions by cumulative time\n")
    f.write("- `arise_profile_top50_self.txt` - Top functions by self time\n")
    f.write("- `arise_system_resources.txt` - System resource usage\n")
    f.write("- `arise_io_analysis.txt` - I/O patterns\n")
    if (output_base / "arise_memory_profile.txt").exists():
        f.write("- `arise_memory_profile.txt` - Memory profiling results\n")

print(f"Summary: {summary_file}")

