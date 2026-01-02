#!/bin/bash
# Minimal MONTAGE profiling - just run and save profile, no fancy reports

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/montage"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

# Defaults
TEST_CASE="${1:-test_montage_full}"
INPUT_PDB="${2:-examples/python.KStar/2RL0.min.reduce.pdb}"
MASTER_MATCHES="${3:-5}"
MAX_FLEX="${4:-4}"

echo "=== Minimal MONTAGE Profiling ==="
echo "Test case: $TEST_CASE"
echo "Input PDB: $INPUT_PDB"
echo "MASTER matches: $MASTER_MATCHES"
echo "Output: $OUTPUT_DIR/$TEST_CASE"
echo

mkdir -p "$OUTPUT_DIR/$TEST_CASE"

# Check prerequisites
cd "$CCKSTAR_DIR"
if [ ! -f "resources/master" ] || [ ! -f "resources/createPDS" ]; then
    echo "ERROR: MASTER prerequisites missing"
    exit 1
fi
chmod +x resources/master resources/createPDS 2>/dev/null || true

# Create temp input dir
TEMP_INPUT_DIR=$(mktemp -d)
trap "rm -rf $TEMP_INPUT_DIR" EXIT
cp "$REPO_ROOT/$INPUT_PDB" "$TEMP_INPUT_DIR/"

# Create minimal Python script
cat > "$OUTPUT_DIR/$TEST_CASE/run_montage.py" << 'PYEOF'
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
PYEOF

# Run it
echo "Running MONTAGE..."
CCKSTAR_DIR="$CCKSTAR_DIR" \
TEMP_INPUT_DIR="$TEMP_INPUT_DIR" \
OUTPUT_DIR="$OUTPUT_DIR" \
TEST_CASE="$TEST_CASE" \
MASTER_MATCHES="$MASTER_MATCHES" \
MAX_FLEX="$MAX_FLEX" \
python3 "$OUTPUT_DIR/$TEST_CASE/run_montage.py" 2>&1 | tee "$OUTPUT_DIR/$TEST_CASE/output.txt"

# Generate basic reports
echo
echo "Generating reports..."
OUTPUT_DIR="$OUTPUT_DIR" TEST_CASE="$TEST_CASE" python3 << 'PYEOF'
import pstats
import sys
import os
from pathlib import Path

output_dir = Path(os.environ['OUTPUT_DIR']) / os.environ['TEST_CASE']
profile_file = output_dir / "montage_profile.prof"

if profile_file.exists():
    stats = pstats.Stats(str(profile_file))
    
    # Top 50 cumulative
    stats.sort_stats('cumulative')
    with open(output_dir / "top50_cumulative.txt", 'w') as f:
        old = sys.stdout
        sys.stdout = f
        stats.print_stats(50)
        sys.stdout = old
    
    # Top 50 time
    stats.sort_stats('time')
    with open(output_dir / "top50_time.txt", 'w') as f:
        old = sys.stdout
        sys.stdout = f
        stats.print_stats(50)
        sys.stdout = old
    
    print("Reports generated:")
    print("  " + str(output_dir / "top50_cumulative.txt"))
    print("  " + str(output_dir / "top50_time.txt"))
else:
    print("ERROR: Profile file not found")
PYEOF

echo
echo "=== Done ==="
echo "Results: $OUTPUT_DIR/$TEST_CASE/"

