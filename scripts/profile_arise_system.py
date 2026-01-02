#!/usr/bin/env python3
"""System resource monitoring for ARISE."""
import sys
import os
import time
import psutil
sys.path.insert(0, os.getcwd())

from ARISE import find_best_scans
import shutil
import glob

output_file = sys.argv[1]

# Monitor system resources during execution
process = psutil.Process()
start_time = time.time()
start_memory = process.memory_info().rss / 1024 / 1024  # MB
start_cpu = process.cpu_percent()

# Track peak memory
peak_memory = start_memory
memory_samples = []
cpu_samples = []

def monitor_resources():
    global peak_memory
    mem_mb = process.memory_info().rss / 1024 / 1024
    cpu_pct = process.cpu_percent(interval=0.1)
    memory_samples.append(mem_mb)
    cpu_samples.append(cpu_pct)
    if mem_mb > peak_memory:
        peak_memory = mem_mb

# Run ARISE with monitoring
temp_infolder = "test_arise_system_input"
outfolder = "test_arise_system_output"
visited_dict = {}

os.makedirs(temp_infolder, exist_ok=True)
os.makedirs(outfolder, exist_ok=True)

# Find matches in current directory or 2RL0-MONTAGE/
match_patterns = ["match*-MONTAGE", "2RL0-MONTAGE/match*-MONTAGE"]
match_dirs = []
for pattern in match_patterns:
    match_dirs.extend(glob.glob(pattern))

for match_dir in match_dirs:
    match_name = os.path.basename(match_dir)
    dest = os.path.join(temp_infolder, match_name)
    if os.path.exists(match_dir):
        shutil.copytree(match_dir, dest, dirs_exist_ok=True)

# Monitor during execution
monitor_resources()
visited_doublets = find_best_scans(temp_infolder, outfolder, visited_dict, apo_tolerance=0.2)
monitor_resources()

end_time = time.time()
end_memory = process.memory_info().rss / 1024 / 1024
end_cpu = process.cpu_percent()

# Write results
with open(output_file, 'w') as f:
    f.write("ARISE System Resource Usage\n")
    f.write("=" * 60 + "\n\n")
    f.write(f"Execution time: {end_time - start_time:.2f} seconds\n")
    f.write(f"Start memory: {start_memory:.2f} MB\n")
    f.write(f"End memory: {end_memory:.2f} MB\n")
    f.write(f"Peak memory: {peak_memory:.2f} MB\n")
    f.write(f"Memory increase: {end_memory - start_memory:.2f} MB\n")
    f.write(f"Average CPU: {sum(cpu_samples)/len(cpu_samples) if cpu_samples else 0:.2f}%\n")
    f.write(f"Matches processed: {len(visited_doublets)}\n")
    f.write(f"\nMemory samples: {len(memory_samples)}\n")
    f.write(f"CPU samples: {len(cpu_samples)}\n")

if os.path.exists(temp_infolder):
    shutil.rmtree(temp_infolder)
if os.path.exists(outfolder):
    shutil.rmtree(outfolder)

