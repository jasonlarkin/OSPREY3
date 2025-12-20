#!/bin/bash
# Add timing code to main branch TestNativeConfEnergyCalculator for benchmarking
# Usage: ./scripts/add_timing_to_main.sh <main_branch_path>

set -e

MAIN_DIR=${1:-../osprey-fork-main}
TEST_FILE="src/test/java/edu/duke/cs/osprey/energy/compiled/TestNativeConfEnergyCalculator.java"

if [ ! -d "$MAIN_DIR" ]; then
    echo "ERROR: Directory not found: $MAIN_DIR"
    exit 1
fi

MAIN_TEST_FILE="$MAIN_DIR/$TEST_FILE"

if [ ! -f "$MAIN_TEST_FILE" ]; then
    echo "ERROR: Test file not found: $MAIN_TEST_FILE"
    exit 1
fi

# Check if timing already exists
if grep -q "log(\"assign:.*confs in" "$MAIN_TEST_FILE"; then
    echo "Timing code already exists in main branch"
    exit 0
fi

# Create backup
cp "$MAIN_TEST_FILE" "$MAIN_TEST_FILE.backup"

# Copy timing code from develop branch
DEVELOP_TEST_FILE="$TEST_FILE"

if [ ! -f "$DEVELOP_TEST_FILE" ]; then
    echo "ERROR: Develop test file not found: $DEVELOP_TEST_FILE"
    exit 1
fi

# Extract timing code sections and apply to main
# This is a simplified approach - manual patch may be needed
echo "Adding timing code to main branch..."
echo "NOTE: This requires manual verification. Check $MAIN_TEST_FILE after running."

# For now, just create a patch file
git diff HEAD -- "$TEST_FILE" > /tmp/timing_patch.diff 2>/dev/null || true

echo "Patch created at /tmp/timing_patch.diff"
echo "Apply manually or use: git apply /tmp/timing_patch.diff (in main branch directory)"

