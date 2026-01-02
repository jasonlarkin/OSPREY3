#!/usr/bin/env python3
"""Analyze cProfile output for ARISE profiling."""
import pstats
import sys
from contextlib import redirect_stdout

prof_file = sys.argv[1]
output_base = sys.argv[2]

stats = pstats.Stats(prof_file)

# Top functions by cumulative time
with open(f'{output_base}_top50_cumulative.txt', 'w') as f:
    with redirect_stdout(f):
        stats.sort_stats('cumulative')
        stats.print_stats(50)

# Top functions by self time
with open(f'{output_base}_top50_self.txt', 'w') as f:
    with redirect_stdout(f):
        stats.sort_stats('tottime')
        stats.print_stats(50)

# By file
with open(f'{output_base}_by_file.txt', 'w') as f:
    with redirect_stdout(f):
        stats.sort_stats('cumulative')
        stats.print_stats(50)

