#!/bin/bash
# Fast K* profiling with relaxed epsilon (0.90-0.95) for faster convergence
# Reduces runtime significantly while still providing useful workload data

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/kstar/workload_variation"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"

mkdir -p "$OUTPUT_DIR"

EPSILON="${1:-0.90}"  # Default to 0.90 for fast profiling

echo "=== Fast K* Profiling (Epsilon=$EPSILON) ==="
echo "Using relaxed epsilon for faster convergence"
echo "Output: $OUTPUT_DIR"
echo ""

cd "$CCKSTAR_DIR"

# Check for 2RL0 MONTAGE output
if [ ! -d "2RL0-MONTAGE" ]; then
    echo "ERROR: 2RL0-MONTAGE directory not found"
    exit 1
fi

MATCH_DIR=$(ls -d 2RL0-MONTAGE/match*-MONTAGE 2>/dev/null | head -1)
if [ -z "$MATCH_DIR" ]; then
    echo "ERROR: No matches found"
    exit 1
fi

KSTAR_DIR="$MATCH_DIR/kstar-[MONTAGE]"

if [ ! -d "$KSTAR_DIR" ]; then
    echo "ERROR: K* directory not found: $KSTAR_DIR"
    exit 1
fi

echo "Using: $KSTAR_DIR"
echo "Epsilon: $EPSILON (default is 0.99, lower = faster)"
echo ""

# Check if already completed
if [ -f "$KSTAR_DIR/submit.out" ] && grep -q "completed" "$KSTAR_DIR/submit.out" 2>/dev/null; then
    echo "K* already completed for this directory"
    exit 0
fi

# Run with relaxed epsilon
PROFILE_DIR="$OUTPUT_DIR/fast_epsilon_${EPSILON}"
mkdir -p "$PROFILE_DIR"

START_TIME=$(date +%s)

echo "=== Running K* with epsilon=$EPSILON ==="

# Use safer memory settings to avoid malloc crashes
# Reduced cores to lower parallel memory pressure
HEAP_MIB="${2:-8192}"  # Default 8GB (was 4GB)
DIRECT_MIB="${3:-4096}"  # Default 4GB (was 2GB)
CPU_CORES="${4:-2}"  # Default 2 cores (was 4)

echo "Memory settings: heap=${HEAP_MIB}MB, direct=${DIRECT_MIB}MB, cores=${CPU_CORES}"
echo ""

# Clean up any stale confdb files that might cause issues
find "$KSTAR_DIR" -name "*.confdb*" -delete 2>/dev/null || true

python3 "$REPO_ROOT/src/main/python/CCKStar/run_kstar_python.py" \
    "$KSTAR_DIR" \
    --epsilon "$EPSILON" \
    --heap-mib "$HEAP_MIB" \
    --direct-mib "$DIRECT_MIB" \
    --cpu-cores "$CPU_CORES" \
    2>&1 | tee "$PROFILE_DIR/kstar_output.txt"

END_TIME=$(date +%s)
DURATION=$((END_TIME - START_TIME))

echo ""
echo "=== Fast Profiling Complete ==="
echo "Duration: ${DURATION}s ($(($DURATION / 60)) minutes)"
echo "Epsilon: $EPSILON"
echo "Results: $PROFILE_DIR"

# Extract metrics
if [ -f "$KSTAR_DIR/submit.out" ]; then
    echo ""
    echo "=== Extracting Metrics ==="
    python3 << PYTHON_EOF
import re
from pathlib import Path

submit_out = Path("$KSTAR_DIR/submit.out")
if submit_out.exists():
    content = submit_out.read_text()
    
    # Extract timing
    time_match = re.search(r'(\d+\.\d+)\s+seconds', content)
    if time_match:
        print(f"K* execution time: {time_match.group(1)}s")
    
    # Extract sequences
    seq_match = re.search(r'(\d+)\s+sequences', content, re.IGNORECASE)
    if seq_match:
        print(f"Sequences processed: {seq_match.group(1)}")
    
    # Check completion
    if "completed" in content.lower():
        print("Status: Completed")
    else:
        print("Status: Incomplete or error")
PYTHON_EOF
fi

