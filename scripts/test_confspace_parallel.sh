#!/bin/bash
# Test parallel ConfSpace compilation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"
PYTHON_DIR="$REPO_ROOT/src/main/python"
cd "$CCKSTAR_DIR"

# Test confspaces (adjust paths as needed)
TARGET="${1:-2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/match1-processed-target-[MONTAGE].confspace}"
DESIGN="${2:-2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/match1-processed-design-[MONTAGE].confspace}"
COMPLEX="${3:-2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/match1-processed-complex-[MONTAGE].confspace}"

if [ ! -f "$TARGET" ] || [ ! -f "$DESIGN" ] || [ ! -f "$COMPLEX" ]; then
    echo "Error: One or more confspace files not found"
    echo "Usage: $0 [target.confspace] [design.confspace] [complex.confspace]"
    exit 1
fi

PYTHON_DIR="$PYTHON_DIR" python3 << EOF
import sys
import os
# Add parent directory to path so we can import CCKStar.KStarPrep
sys.path.insert(0, os.environ.get('PYTHON_DIR', '.'))
# Change to CCKStar directory so relative imports work
os.chdir(os.path.join(os.environ.get('PYTHON_DIR', '.'), 'CCKStar'))
import time
import osprey
osprey.start()
from CCKStar.KStarPrep import compile_confspaces

spaces = ["$TARGET", "$DESIGN", "$COMPLEX"]

# Sequential
print("=== Sequential Compilation ===")
start = time.time()
with osprey.prep.LocalService():
    compile_confspaces(spaces, parallel=False)
sequential_time = time.time() - start
print(f"Sequential time: {sequential_time:.2f}s\n")

# Parallel
print("=== Parallel Compilation ===")
start = time.time()
with osprey.prep.LocalService():
    compile_confspaces(spaces, parallel=True)
parallel_time = time.time() - start
print(f"Parallel time: {parallel_time:.2f}s")
print(f"Speedup: {sequential_time/parallel_time:.2f}x")
print(f"Time saved: {sequential_time - parallel_time:.2f}s")
EOF

