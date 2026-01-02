#!/usr/bin/env python3
"""I/O analysis for ARISE."""
import sys
import os
import time
sys.path.insert(0, os.getcwd())

import glob

output_file = sys.argv[1]

# Analyze I/O patterns
io_stats = {
    'files_read': 0,
    'total_bytes_read': 0,
    'files_written': 0,
    'total_bytes_written': 0,
    'read_times': [],
    'write_times': []
}

# Count files processed
match_patterns = ["match*-MONTAGE", "2RL0-MONTAGE/match*-MONTAGE"]
match_dirs = []
for pattern in match_patterns:
    match_dirs.extend(glob.glob(pattern))

for match_dir in match_dirs:
    kstar_dir = os.path.join(match_dir, "kstar-[MONTAGE]")
    submit_out = os.path.join(kstar_dir, "submit.out")
    if os.path.exists(submit_out):
        io_stats['files_read'] += 1
        size = os.path.getsize(submit_out)
        io_stats['total_bytes_read'] += size
        
        # Time to read
        start = time.time()
        with open(submit_out, 'r') as f:
            _ = f.read()
        io_stats['read_times'].append(time.time() - start)

with open(output_file, 'w') as f:
    f.write("ARISE I/O Analysis\n")
    f.write("=" * 60 + "\n\n")
    f.write(f"Files read: {io_stats['files_read']}\n")
    f.write(f"Total bytes read: {io_stats['total_bytes_read']} ({io_stats['total_bytes_read']/1024:.2f} KB)\n")
    f.write(f"Average read time: {sum(io_stats['read_times'])/len(io_stats['read_times']) if io_stats['read_times'] else 0:.4f} seconds\n")
    f.write(f"Total read time: {sum(io_stats['read_times']):.4f} seconds\n")

