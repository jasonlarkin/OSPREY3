#!/bin/bash
# Performance Benchmarking Script
# Compares performance between two repositories or branches
# Integrates with existing performance tools in scripts/tools/
# 
# Usage:
#   # Option 1: Compare two separate repository directories
#   ./scripts/benchmark_performance.sh /path/to/osprey-fork-main /path/to/osprey-fork_modern
# 
#   # Option 2: Compare branches in current repository (requires clean working tree)
#   ./scripts/benchmark_performance.sh --branch main develop
# 
# Outputs:
# - benchmark_results/ directory with test outputs and comparisons
# - JSON comparison report
# - CSV format compatible with scripts/tools/summarize_benchmark_results.py

set -euo pipefail

OUTPUT_DIR="benchmark_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

# Parse arguments
if [ "$1" = "--branch" ]; then
    # Branch comparison mode
    BASELINE_BRANCH=${2:-main}
    TEST_BRANCH=${3:-develop}
    BASELINE_DIR=""
    TEST_DIR=""
    MODE="branch"
    echo "=== Performance Benchmarking (Branch Mode) ==="
    echo "Baseline branch: $BASELINE_BRANCH"
    echo "Test branch: $TEST_BRANCH"
else
    # Directory comparison mode
    BASELINE_DIR=${1:-../osprey-fork-main}
    TEST_DIR=${2:-.}
    MODE="directory"
    echo "=== Performance Benchmarking (Directory Mode) ==="
    echo "Baseline directory: $BASELINE_DIR"
    echo "Test directory: $TEST_DIR"
fi

echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Function to extract timing metrics from test output
extract_timings() {
    local output_file=$1
    local branch_name=$2
    local results_file="$OUTPUT_DIR/${branch_name}_timings_${TIMESTAMP}.txt"
    
    echo "Extracting timings from $branch_name..."
    
    # Extract timing lines (format: "operation: N confs in X.XXs (Y.YY confs/s)")
    grep -E "(assign|calcEnergy_all|minimizeEnergy_all|minimize_all):.*confs in" "$output_file" > "$results_file" || true
    
    # Also extract summary timing if available
    if grep -q "confs/s" "$output_file"; then
        echo "" >> "$results_file"
        echo "=== Summary ===" >> "$results_file"
        grep "confs/s" "$output_file" >> "$results_file" || true
    fi
    
    echo "Timings saved to: $results_file"
    return 0
}

# Function to run tests in a directory
run_benchmark_in_dir() {
    local dir=$1
    local label=$2
    local original_dir=$(pwd)
    local output_file="$original_dir/$OUTPUT_DIR/${label}_test_output_${TIMESTAMP}.txt"
    
    echo "=== Running tests in: $dir (label: $label) ==="
    
    # Change to target directory
    if [ ! -d "$dir" ]; then
        echo "ERROR: Directory not found: $dir"
        exit 1
    fi
    
    cd "$dir" || {
        echo "ERROR: Failed to change to directory: $dir"
        exit 1
    }
    
    # Build C++ library
    echo "Building C++ library (this may take 1-2 minutes on first run)..."
    if [ -d "src/main/cc/ConfEcalc" ]; then
        cd src/main/cc/ConfEcalc
        # Only rebuild if build doesn't exist or is older than source
        if [ ! -d "build" ] || [ "src/main/cc/ConfEcalc" -nt "build/libConfEcalc.so" ] 2>/dev/null; then
            rm -rf build
            mkdir -p build
            cd build
            cmake .. > /dev/null 2>&1
            cmake --build . -j$(nproc) > /dev/null 2>&1
            cd ..
        else
            echo "C++ library already built, skipping rebuild"
        fi
    else
        echo "WARNING: src/main/cc/ConfEcalc not found, skipping C++ build"
    fi
    
    # Ensure we're back in the target directory (after C++ build navigation)
    cd "$dir" || {
        echo "ERROR: Failed to return to directory: $dir"
        cd "$original_dir"
        exit 1
    }
    
    # Verify we're in the right place
    if [ ! -f "gradlew" ] && [ ! -f "build.gradle.kts" ]; then
        echo "ERROR: Not in project root. Current directory: $(pwd)"
        echo "Expected gradlew in: $dir"
        cd "$original_dir"
        exit 1
    fi
    
    # Run tests and capture output
    # Use specific fast test methods for performance benchmarking (10-20s instead of hours)
    # See docs/testing/FAST_TESTS_FOR_DEVELOPMENT.md and notes/simd/SIMD_BENCHMARKING.md
    echo "Running performance benchmark tests (Gradle build may take 2-5 minutes on first run, then ~10-20s for tests)..."
    if [ -f "gradlew" ]; then
        # Use unbuffered output and ensure tee completes
        ./gradlew test --rerun-tasks -PshowTestOutput=true \
            --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
            --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64" \
            --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64" \
            --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.assign_native_2RL0_f64" \
            --no-daemon 2>&1 | tee "$output_file" || {
            echo "ERROR: Gradle test command failed or was interrupted"
            cd "$original_dir"
            exit 1
        }
        
        # Verify only 4 tests ran
        test_count=$(grep -c "TestNativeConfEnergyCalculator > .* PASSED" "$output_file" 2>/dev/null || echo "0")
        if [ "$test_count" -ne 4 ]; then
            echo "WARNING: Expected 4 tests, found $test_count. Check if full test suite ran."
        fi
    else
        echo "ERROR: gradlew not found in $dir"
        cd "$original_dir"
        exit 1
    fi
    
    # Extract timings (using original directory for output)
    cd "$original_dir"
    extract_timings "$output_file" "$label"
    
    echo "Benchmark completed for $label"
    echo ""
}

# Function to run tests on a branch (for branch mode)
run_benchmark_branch() {
    local branch=$1
    local output_file="$OUTPUT_DIR/${branch}_test_output_${TIMESTAMP}.txt"
    
    echo "=== Running tests on branch: $branch ==="
    
    # Checkout branch
    git checkout "$branch" || {
        echo "ERROR: Failed to checkout branch $branch"
        echo "Hint: Stash or commit your changes first, or use directory mode"
        exit 1
    }
    
    # Build C++ library
    echo "Building C++ library (this may take 1-2 minutes on first run)..."
    cd src/main/cc/ConfEcalc
    if [ ! -d "build" ] || [ "src/main/cc/ConfEcalc" -nt "build/libConfEcalc.so" ] 2>/dev/null; then
        rm -rf build
        mkdir -p build
        cd build
        cmake .. > /dev/null 2>&1
        cmake --build . -j$(nproc) > /dev/null 2>&1
        cd ..
    else
        echo "C++ library already built, skipping rebuild"
    fi
    cd ../../../../..
    
    # Run tests and capture output
    # Use specific fast test methods for performance benchmarking (10-20s instead of hours)
    # See docs/testing/FAST_TESTS_FOR_DEVELOPMENT.md and notes/simd/SIMD_BENCHMARKING.md
    echo "Running performance benchmark tests (Gradle build may take 2-5 minutes on first run, then ~10-20s for tests)..."
    ./gradlew test --rerun-tasks -PshowTestOutput=true \
        --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
        --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64" \
        --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64" \
        --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.assign_native_2RL0_f64" \
        --no-daemon 2>&1 | tee "$output_file"
    
    # Verify only 4 tests ran
    test_count=$(grep -c "TestNativeConfEnergyCalculator > .* PASSED" "$output_file" 2>/dev/null || echo "0")
    if [ "$test_count" -ne 4 ]; then
        echo "WARNING: Expected 4 tests, found $test_count. Check if full test suite ran."
    fi
    
    # Extract timings
    extract_timings "$output_file" "$branch"
    
    echo "Benchmark completed for $branch"
    echo ""
}

# Run benchmarks
echo "Starting performance comparison..."

if [ "$MODE" = "branch" ]; then
    run_benchmark_branch "$BASELINE_BRANCH"
    run_benchmark_branch "$TEST_BRANCH"
    BASELINE_LABEL="$BASELINE_BRANCH"
    TEST_LABEL="$TEST_BRANCH"
    BASELINE_DISPLAY="$BASELINE_BRANCH"
    TEST_DISPLAY="$TEST_BRANCH"
else
    # Use absolute paths for clarity
    BASELINE_ABS=$(cd "$BASELINE_DIR" && pwd)
    TEST_ABS=$(cd "$TEST_DIR" && pwd)
    
    run_benchmark_in_dir "$BASELINE_ABS" "baseline"
    run_benchmark_in_dir "$TEST_ABS" "test"
    BASELINE_LABEL="baseline"
    TEST_LABEL="test"
    BASELINE_DISPLAY="$BASELINE_DIR"
    TEST_DISPLAY="$TEST_DIR"
fi

# Compare results
echo "=== Performance Comparison ==="
BASELINE_FILE="$OUTPUT_DIR/${BASELINE_LABEL}_timings_${TIMESTAMP}.txt"
TEST_FILE="$OUTPUT_DIR/${TEST_LABEL}_timings_${TIMESTAMP}.txt"

if [ ! -f "$BASELINE_FILE" ] || [ ! -f "$TEST_FILE" ]; then
    echo "ERROR: Could not find timing files"
    exit 1
fi

echo ""
echo "Baseline ($BASELINE_DISPLAY):"
cat "$BASELINE_FILE"
echo ""
echo "Test ($TEST_DISPLAY):"
cat "$TEST_FILE"
echo ""

# Create comparison report using Python script
echo "=== Generating Performance Comparison ==="
COMPARISON_JSON="$OUTPUT_DIR/comparison_${TIMESTAMP}.json"
COMPARISON_CSV="$OUTPUT_DIR/comparison_${TIMESTAMP}.csv"

python3 scripts/extract_performance_metrics.py \
    "$OUTPUT_DIR/${BASELINE_LABEL}_test_output_${TIMESTAMP}.txt" \
    "$OUTPUT_DIR/${TEST_LABEL}_test_output_${TIMESTAMP}.txt" \
    "$COMPARISON_JSON" \
    "$COMPARISON_CSV" || {
    echo "Warning: Performance comparison completed with issues"
}

# Create text summary
COMPARISON_FILE="$OUTPUT_DIR/comparison_${TIMESTAMP}.txt"
echo "=== Performance Comparison Report ===" > "$COMPARISON_FILE"
echo "Baseline: $BASELINE_DISPLAY" >> "$COMPARISON_FILE"
echo "Test: $TEST_DISPLAY" >> "$COMPARISON_FILE"
echo "Timestamp: $TIMESTAMP" >> "$COMPARISON_FILE"
echo "" >> "$COMPARISON_FILE"
echo "=== Baseline Timings ===" >> "$COMPARISON_FILE"
cat "$BASELINE_FILE" >> "$COMPARISON_FILE"
echo "" >> "$COMPARISON_FILE"
echo "=== Test Timings ===" >> "$COMPARISON_FILE"
cat "$TEST_FILE" >> "$COMPARISON_FILE"

echo ""
echo "=== Benchmarking Complete ==="
echo "Results saved to: $OUTPUT_DIR/"
echo "  - Text summary: $COMPARISON_FILE"
echo "  - JSON report: $COMPARISON_JSON"
echo "  - CSV data: $COMPARISON_CSV"
echo ""
echo "To analyze CSV with existing tools:"
echo "  python3 scripts/tools/summarize_benchmark_results.py $COMPARISON_CSV"

