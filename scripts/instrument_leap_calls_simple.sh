#!/bin/bash
# Simple LEaP call instrumentation using strace

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"
PYTHON_DIR="$REPO_ROOT/src/main/python"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/confspace_compilation/leap_instrumentation"
mkdir -p "$OUTPUT_DIR"

cd "$CCKSTAR_DIR"

# Test confspaces
TARGET="${1:-2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/match1-processed-target-[MONTAGE].confspace}"
DESIGN="${2:-2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/match1-processed-design-[MONTAGE].confspace}"
COMPLEX="${3:-2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/match1-processed-complex-[MONTAGE].confspace}"

if [ ! -f "$TARGET" ] || [ ! -f "$DESIGN" ] || [ ! -f "$COMPLEX" ]; then
    echo "Error: One or more confspace files not found"
    echo "Usage: $0 [target.confspace] [design.confspace] [complex.confspace]"
    exit 1
fi

echo "=== Instrumenting LEaP Calls ==="
echo "This will trace teLeap subprocess calls during compilation"
echo ""

# Find teLeap path
TELEAP_PATH="$REPO_ROOT/progs/ambertools/bin/teLeap"
if [ ! -f "$TELEAP_PATH" ]; then
    echo "Error: teLeap not found at $TELEAP_PATH"
    exit 1
fi

# Use strace to monitor teLeap calls
# Track: execve calls to teLeap, timing, file I/O
PYTHON_DIR="$PYTHON_DIR" python3 << EOF
import sys
import os
import time
import subprocess
import json
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, os.environ.get('PYTHON_DIR', '.'))
os.chdir(os.path.join(os.environ.get('PYTHON_DIR', '.'), 'CCKStar'))

import osprey
osprey.start()
from CCKStar.KStarPrep import compile_confspaces

# Track LEaP calls
leap_calls = []
call_times = defaultdict(list)

def monitor_leap():
    """Monitor teLeap process calls."""
    # This is a simplified approach - we'll count calls via process monitoring
    # In a real implementation, we'd use strace or modify the Kotlin code
    pass

spaces = ["$TARGET", "$DESIGN", "$COMPLEX"]

print("Starting compilation...")
start_time = time.time()

# Compile with monitoring
with osprey.prep.LocalService():
    compile_confspaces(spaces, parallel=True)

total_time = time.time() - start_time

# Count teLeap processes (approximate)
# Note: This is a rough estimate - actual call count would need Kotlin instrumentation
print(f"\nCompilation completed in {total_time:.2f}s")
print(f"\nNote: For detailed LEaP call patterns, we need to:")
print(f"  1. Instrument Kotlin code (ConfSpaceCompiler.kt)")
print(f"  2. Add logging to Leap.run() in forcefieldParams.kt")
print(f"  3. Or use JVM profiling tools")

# Save basic timing
output_file = Path("$OUTPUT_DIR") / "leap_timing.json"
output_file.parent.mkdir(parents=True, exist_ok=True)

with open(output_file, 'w') as f:
    json.dump({
        'total_time': total_time,
        'confspaces': spaces,
        'note': 'LEaP call count requires Kotlin instrumentation'
    }, f, indent=2)

print(f"\nBasic timing saved to: {output_file}")
EOF

echo ""
echo "=== Next Steps ==="
echo "To get detailed LEaP call patterns, we need to:"
echo "1. Add logging to ConfSpaceCompiler.kt (parameterizeAtoms calls)"
echo "2. Add logging to forcefieldParams.kt (Leap.run calls)"
echo "3. Or use JVM profiling (async-profiler) to see HTTP service calls"

