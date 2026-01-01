import sys
import os
import cProfile
import time
from pathlib import Path

cckstar_dir = Path(os.environ['CCKSTAR_DIR'])
sys.path.insert(0, str(cckstar_dir))
os.chdir(str(cckstar_dir))

# Cleanup existing dirs
import glob
import shutil
for d in glob.glob("*-MONTAGE") + glob.glob("*-MASTER"):
    if os.path.isdir(d):
        shutil.rmtree(d)

from MONTAGE import run_MONTAGE

profiler = cProfile.Profile()
profiler.enable()

start = time.time()
try:
    run_MONTAGE(
        os.environ['TEMP_INPUT_DIR'],
        "L", "D",
        int(os.environ['MASTER_MATCHES']),
        int(os.environ['MAX_FLEX'])
    )
except Exception as e:
    print(f"ERROR: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc()

duration = time.time() - start
profiler.disable()

profile_file = Path(os.environ['OUTPUT_DIR']) / os.environ['TEST_CASE'] / "montage_profile.prof"
profiler.dump_stats(str(profile_file))

print(f"\nCompleted in {duration:.1f}s")
print(f"Profile: {profile_file}")
