#!/usr/bin/env python3
"""Memory profiling for ARISE."""
import sys
import os
sys.path.insert(0, os.getcwd())

from memory_profiler import profile
from ARISE import find_best_scans
import shutil
import glob

@profile
def profile_arise():
    temp_infolder = "test_arise_memory_input"
    outfolder = "test_arise_memory_output"
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
    
    visited_doublets = find_best_scans(temp_infolder, outfolder, visited_dict, apo_tolerance=0.2)
    
    if os.path.exists(temp_infolder):
        shutil.rmtree(temp_infolder)
    if os.path.exists(outfolder):
        shutil.rmtree(outfolder)

if __name__ == "__main__":
    profile_arise()

