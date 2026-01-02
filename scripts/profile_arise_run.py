#!/usr/bin/env python3
"""Run ARISE for profiling."""
import sys
import os
sys.path.insert(0, os.getcwd())

from ARISE import find_best_scans
import shutil
import glob

# Create temp directory with all matches for testing
temp_infolder = "test_arise_profiling_input"
outfolder = "test_arise_profiling_output"
visited_dict = {}

# Copy all matches to temp directory
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

# Run ARISE processing
print(f"Processing {len(glob.glob(temp_infolder + '/match*-MONTAGE'))} matches...")
visited_doublets = find_best_scans(temp_infolder, outfolder, visited_dict, apo_tolerance=0.2)
print(f"Processed {len(visited_doublets)} matches")

# Cleanup
if os.path.exists(temp_infolder):
    shutil.rmtree(temp_infolder)
if os.path.exists(outfolder):
    shutil.rmtree(outfolder)

