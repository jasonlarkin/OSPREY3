#!/usr/bin/env python3
import sys
import os
import cProfile
import pstats
import time
import subprocess
from pathlib import Path

# Resolve repo root relative to this script so paths are portable across machines.
# File location: <repo_root>/pipeline_analysis/montage/test_montage_full/run_montage_profiled.py
repo_root = Path(__file__).resolve().parents[3]
cckstar_dir = (repo_root / "src/main/python/CCKStar").resolve()
sys.path.insert(0, str(cckstar_dir))

# Change to CCKStar directory for relative imports and resource access
os.chdir(str(cckstar_dir))

# Clean up any existing MONTAGE directories from previous runs
import glob
import shutil
for montage_dir in glob.glob("*-MONTAGE"):
    if os.path.isdir(montage_dir):
        print(f"Cleaning up existing directory: {montage_dir}")
        shutil.rmtree(montage_dir)
for master_dir in glob.glob("*-MASTER"):
    if os.path.isdir(master_dir):
        print(f"Cleaning up existing directory: {master_dir}")
        shutil.rmtree(master_dir)

from MONTAGE import run_MONTAGE

# Configuration
input_pdb_dir = "/tmp/tmp.zbyGnQY0SU"
input_chirality = "L"
output_chirality = "D"
master_matches = 5
max_flex = 4

print(f"Input PDB directory: {input_pdb_dir}")
print(f"MASTER matches: {master_matches}")
print(f"Max flex: {max_flex}")
print()

# Profile the run
profiler = cProfile.Profile()
profiler.enable()

start_time = time.time()

try:
    run_MONTAGE(
        input_pdb_dir,
        input_chirality,
        output_chirality,
        master_matches,
        max_flex
    )
except shutil.Error as e:
    # Handle directory already exists error (non-fatal)
    if "already exists" in str(e):
        print(f"WARNING: {e}", file=sys.stderr)
        print("Continuing - profile data was captured", file=sys.stderr)
    else:
        raise
except Exception as e:
    print(f"ERROR: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc()
    # Don't exit - we still want to save the profile

end_time = time.time()
duration = end_time - start_time

profiler.disable()

# Save profile
profile_file = Path(__file__).parent / "montage_profile.prof"
profiler.dump_stats(str(profile_file))

print(f"\nMONTAGE completed in {duration:.2f} seconds")
print(f"Profile saved to: {profile_file}")
