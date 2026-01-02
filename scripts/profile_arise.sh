#!/bin/bash
# Profile ARISE iterative design stage
# Captures: CPU usage, memory patterns, I/O, timing

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/arise"

# Default test case
TEST_CASE="${1:-test_arise_10_matches}"
OUTPUT_BASE="$OUTPUT_DIR/$TEST_CASE"

mkdir -p "$OUTPUT_DIR"
mkdir -p "$OUTPUT_BASE"

echo "=== ARISE Profiling ==="
echo "Test case: $TEST_CASE"
echo "Output: $OUTPUT_BASE"
echo ""

cd "$CCKSTAR_DIR"

# Check for matches in current directory or 2RL0-MONTAGE/
MATCH_DIR=""
if [ -d "2RL0-MONTAGE" ] && [ -d "2RL0-MONTAGE/match1-MONTAGE" ]; then
    MATCH_DIR="2RL0-MONTAGE"
    MATCH_PATTERN="2RL0-MONTAGE/match*-MONTAGE"
elif [ -d "match1-MONTAGE" ]; then
    MATCH_DIR="."
    MATCH_PATTERN="match*-MONTAGE"
else
    echo "ERROR: No matches found in $CCKSTAR_DIR/"
    echo "Expected either ./match*-MONTAGE or ./2RL0-MONTAGE/match*-MONTAGE"
    echo "Run convert_example_results_for_arise.py first"
    exit 1
fi

MATCH_COUNT=$(ls -d $MATCH_PATTERN 2>/dev/null | wc -l)
echo "Found $MATCH_COUNT matches to process in $MATCH_DIR"
echo ""

# === 1. Python cProfile (CPU hotspots) ===
echo "=== 1. Python cProfile ==="
python3 -m cProfile -o "$OUTPUT_BASE/arise_profile.prof" \
    "$SCRIPT_DIR/profile_arise_run.py" 2>&1 | tee "$OUTPUT_BASE/arise_cprofile_output.txt"

# Analyze profile
python3 "$SCRIPT_DIR/profile_arise_analyze.py" \
    "$OUTPUT_BASE/arise_profile.prof" \
    "$OUTPUT_BASE/arise_profile"

echo "  Profile saved: $OUTPUT_BASE/arise_profile.prof"
echo "  Top functions: $OUTPUT_BASE/arise_profile_top50_cumulative.txt"
echo ""

# === 2. Memory Profiling ===
echo "=== 2. Memory Profiling ==="
cd "$CCKSTAR_DIR"
if command -v python3 -m memory_profiler &> /dev/null; then
    python3 "$SCRIPT_DIR/profile_arise_memory.py" > "$OUTPUT_BASE/arise_memory_profile.txt" 2>&1
    echo "  Memory profile: $OUTPUT_BASE/arise_memory_profile.txt"
else
    echo "  memory_profiler not available, skipping"
fi
echo ""

# === 3. System Resource Monitoring ===
echo "=== 3. System Resource Monitoring ==="
cd "$CCKSTAR_DIR"
python3 "$SCRIPT_DIR/profile_arise_system.py" "$OUTPUT_BASE/arise_system_resources.txt"
echo "  System resources: $OUTPUT_BASE/arise_system_resources.txt"
echo ""

# === 4. I/O Analysis ===
echo "=== 4. I/O Analysis ==="
cd "$CCKSTAR_DIR"
python3 "$SCRIPT_DIR/profile_arise_io.py" "$OUTPUT_BASE/arise_io_analysis.txt"
echo "  I/O analysis: $OUTPUT_BASE/arise_io_analysis.txt"
echo ""

# === 5. Generate Summary ===
echo "=== 5. Generating Summary Report ==="
python3 "$SCRIPT_DIR/profile_arise_summary.py" "$OUTPUT_BASE"

echo ""
echo "=== ARISE Profiling Complete ==="
echo "Results saved to: $OUTPUT_BASE"
echo ""
echo "Files generated:"
echo "  - arise_profile.prof (cProfile binary)"
echo "  - arise_profile_top50_*.txt (top functions)"
echo "  - arise_system_resources.txt (system metrics)"
echo "  - arise_io_analysis.txt (I/O patterns)"
echo "  - arise_profiling_summary.md (summary report)"
