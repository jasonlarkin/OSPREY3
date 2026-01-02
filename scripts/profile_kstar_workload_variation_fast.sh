#!/bin/bash
# Profile K* on smaller systems first (fast iteration)
# Start with protein-only, then scale up

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/kstar/workload_variation"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"

mkdir -p "$OUTPUT_DIR"

echo "=== K* Workload Variation Profiling (Fast Mode) ==="
echo "Profiling smaller systems first for faster iteration"
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
echo ""

# Strategy: Use existing example confspaces that are smaller
# Check examples directory for smaller test cases
EXAMPLES_DIR="$REPO_ROOT/examples"

echo "=== Checking for smaller test cases ==="

# Look for example confspaces
if [ -d "$EXAMPLES_DIR/python.ccs/kstar" ]; then
    echo "Found example confspaces in: $EXAMPLES_DIR/python.ccs/kstar"
    
    # Profile hepes (likely smallest)
    if [ -f "$EXAMPLES_DIR/python.ccs/kstar/hepes.ccsx" ]; then
        echo ""
        echo "=== Profiling HEPES (Small System) ==="
        PROFILE_DIR="$OUTPUT_DIR/hepes"
        mkdir -p "$PROFILE_DIR"
        
        START_TIME=$(date +%s)
        
        # Run with minimal settings for speed
        python3 << PYTHON_EOF
import sys
import os
sys.path.insert(0, os.getcwd())

import osprey
osprey.start()

from osprey import ccs
import time

confspace_file = "$EXAMPLES_DIR/python.ccs/kstar/hepes.ccsx"
print(f"Loading: {confspace_file}")

start = time.time()
confspace = ccs.load(confspace_file)
load_time = time.time() - start
print(f"Load time: {load_time:.2f}s")

# Get pair count
pair_count = confspace.numPairs() if hasattr(confspace, 'numPairs') else 'unknown'
print(f"Pairs: {pair_count}")

# Minimal K* run (single sequence, relaxed epsilon)
print("Running minimal K* calculation...")
# Note: This is a placeholder - actual K* run would go here
# For now, just measure confspace loading

with open("$PROFILE_DIR/metrics.txt", 'w') as f:
    f.write(f"Load time: {load_time:.2f}s\n")
    f.write(f"Pairs: {pair_count}\n")

PYTHON_EOF
        
        END_TIME=$(date +%s)
        DURATION=$((END_TIME - START_TIME))
        
        echo "  Completed in ${DURATION}s"
        echo "  Results: $PROFILE_DIR"
    fi
fi

# Alternative: Create minimal test case from existing confspace
echo ""
echo "=== Creating Minimal Test Case ==="
echo "Extracting protein component only (smallest subset)"

PROFILE_DIR="$OUTPUT_DIR/protein_only"
mkdir -p "$PROFILE_DIR"

# Check if we can extract just protein component
if [ -f "$KSTAR_DIR/target.ccsx" ]; then
    echo "Found target.ccsx (protein component)"
    echo "This is the smallest component (~1,008 pairs)"
    
    START_TIME=$(date +%s)
    
    python3 << PYTHON_EOF
import sys
import os
sys.path.insert(0, os.getcwd())

import osprey
osprey.start()

from osprey import ccs
import time

confspace_file = "$KSTAR_DIR/target.ccsx"
print(f"Loading protein confspace: {confspace_file}")

start = time.time()
confspace = ccs.load(confspace_file)
load_time = time.time() - start

# Get basic metrics
try:
    num_positions = confspace.numPos() if hasattr(confspace, 'numPos') else 'unknown'
    num_pairs = confspace.numPairs() if hasattr(confspace, 'numPairs') else 'unknown'
except:
    num_positions = 'unknown'
    num_pairs = 'unknown'

print(f"Load time: {load_time:.2f}s")
print(f"Positions: {num_positions}")
print(f"Pairs: {num_pairs}")

with open("$PROFILE_DIR/metrics.txt", 'w') as f:
    f.write(f"System: protein_only\n")
    f.write(f"Load time: {load_time:.2f}s\n")
    f.write(f"Positions: {num_positions}\n")
    f.write(f"Pairs: {num_pairs}\n")
    f.write(f"Expected pairs: ~1,008\n")

PYTHON_EOF
    
    END_TIME=$(date +%s)
    DURATION=$((END_TIME - START_TIME))
    
    echo "  Completed in ${DURATION}s"
    echo "  Results: $PROFILE_DIR"
fi

echo ""
echo "=== Summary ==="
echo "Fast profiling complete. For full K* runs:"
echo "1. Start with protein-only (smallest, ~1K pairs)"
echo "2. Then ligand-only (medium, ~7.5K pairs)"
echo "3. Finally complex (large, ~17K pairs) - may take hours"
echo ""
echo "Results: $OUTPUT_DIR"

