#!/bin/bash
# Profile full K* calculations with combined JVM, native, and GC profiling
# Combines async-profiler (JVM), perf (native C++), and GC logging
# Designed for WSL/Linux environment

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="${OUTPUT_DIR:-$PROJECT_ROOT/pipeline_analysis/full_kstar}"

# Default test method
TEST_METHOD="${1:-test2RL0}"
TEST_CLASS="edu.duke.cs.osprey.kstar.TestKStar"
FULL_TEST_NAME="$TEST_CLASS.$TEST_METHOD"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=== Full K* Profiling ==="
echo "Project root: $PROJECT_ROOT"
echo "Output directory: $OUTPUT_DIR"
echo "Test: $FULL_TEST_NAME"
echo ""

# Check prerequisites
if [ ! -f "$PROJECT_ROOT/gradlew" ]; then
    echo "ERROR: gradlew not found in $PROJECT_ROOT"
    exit 1
fi

cd "$PROJECT_ROOT" || exit 1
chmod +x gradlew 2>/dev/null || true

# Check for profiling tools
USE_PERF=false
if command -v perf &> /dev/null; then
    USE_PERF=true
    perf --version 2>/dev/null || true
    echo "perf found"
else
    echo "WARNING: perf not found (native profiling will be limited)"
fi

USE_ASYNC_PROFILER=false
ASYNC_PROFILER_DIR="${ASYNC_PROFILER_DIR:-}"
if [ -n "$ASYNC_PROFILER_DIR" ] && [ -f "$ASYNC_PROFILER_DIR/libasyncProfiler.so" ]; then
    USE_ASYNC_PROFILER=true
    echo "async-profiler found at $ASYNC_PROFILER_DIR"
elif command -v async-profiler &> /dev/null; then
    USE_ASYNC_PROFILER=true
    echo "async-profiler found in PATH"
else
    echo "WARNING: async-profiler not found (JVM profiling will be limited)"
    echo "  Install with: ./scripts/setup_profiling_tools.sh"
fi

# Create test-specific output directory
TEST_OUTPUT_DIR="$OUTPUT_DIR/$TEST_METHOD"
mkdir -p "$TEST_OUTPUT_DIR"

echo ""
echo "=== Step 1: Setting up profiling environment ==="
echo ""

# Clear any existing JVM options to avoid conflicts
unset JAVA_TOOL_OPTIONS
unset GRADLE_OPTS
unset JAVA_OPTS

# Setup GC logging (Java 17 unified logging)
GC_LOG_FILE="$TEST_OUTPUT_DIR/gc.log"
GC_LOG_OPTS="-Xlog:gc*:file=$GC_LOG_FILE:time,tags:filecount=5,filesize=50M"

# Setup async-profiler if available
ASYNC_PROFILER_OPTS=""
if [ "$USE_ASYNC_PROFILER" = true ]; then
    if [ -n "$ASYNC_PROFILER_DIR" ] && [ -f "$ASYNC_PROFILER_DIR/libasyncProfiler.so" ]; then
        ASYNC_PROFILER_OPTS="-agentpath:$ASYNC_PROFILER_DIR/libasyncProfiler.so=start,event=cpu,file=$TEST_OUTPUT_DIR/async_profiler_cpu.html"
    elif command -v async-profiler &> /dev/null; then
        # Try to find async-profiler library
        ASYNC_PROFILER_LIB=$(find /usr/local /opt ~ -name "libasyncProfiler.so" 2>/dev/null | head -1)
        if [ -n "$ASYNC_PROFILER_LIB" ]; then
            ASYNC_PROFILER_OPTS="-agentpath:$ASYNC_PROFILER_LIB=start,event=cpu,file=$TEST_OUTPUT_DIR/async_profiler_cpu.html"
        fi
    fi
fi

# Combine JVM options (Gradle uses GRADLE_OPTS, not JAVA_OPTS)
GRADLE_OPTS="-Xmx4g -XX:+UseG1GC $GC_LOG_OPTS $ASYNC_PROFILER_OPTS"
export GRADLE_OPTS

echo "JVM Options (GRADLE_OPTS): $GRADLE_OPTS"
echo "GC Log: $GC_LOG_FILE"
if [ "$USE_ASYNC_PROFILER" = true ]; then
    echo "Async-profiler: Enabled (CPU profile)"
fi
echo ""

# Build the test command
TEST_CMD="./gradlew test --tests \"$FULL_TEST_NAME\" --no-daemon"

echo "=== Step 2: Running test with profiling ==="
echo "Command: $TEST_CMD"
echo ""

# First, verify the test works without profiling
echo "Verifying test command works..."
if ! bash -c "$TEST_CMD" > "$TEST_OUTPUT_DIR/test_verify.txt" 2>&1; then
    echo "ERROR: Test command failed. Check: $TEST_OUTPUT_DIR/test_verify.txt"
    echo "Last 50 lines:"
    tail -50 "$TEST_OUTPUT_DIR/test_verify.txt"
    exit 1
fi
echo "Test command verified"
echo ""

# Run with perf if available (captures both JVM and native code)
if [ "$USE_PERF" = true ]; then
    echo "Running with perf stat (captures JVM + native C++ code)..."
    echo ""
    
    # Perf stat for overall statistics
    # Note: perf stat will show warnings but still work in WSL
    # Capture both filtered and unfiltered output
    perf stat -o "$TEST_OUTPUT_DIR/perf_stat.txt" \
        -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,page-faults \
        -e L1-dcache-loads,L1-dcache-load-misses,LLC-loads,LLC-load-misses \
        bash -c "$TEST_CMD" \
        > "$TEST_OUTPUT_DIR/test_output_raw.txt" 2>&1
    
    TEST_EXIT_CODE=$?
    
    # Filter warnings but keep errors
    grep -vE "(WARNING: perf not found|You may need to install|You may also want to install|linux-tools-|linux-cloud-tools-)" \
        "$TEST_OUTPUT_DIR/test_output_raw.txt" > "$TEST_OUTPUT_DIR/test_output.txt" || true
    
    if [ "$TEST_EXIT_CODE" -ne 0 ]; then
        echo "WARNING: Test exit code: $TEST_EXIT_CODE"
        echo "Check full output: $TEST_OUTPUT_DIR/test_output_raw.txt"
    fi
    
    # Perf record for detailed profiling (run separately to avoid interference)
    if [ "${TEST_EXIT_CODE:-0}" -eq 0 ]; then
        echo ""
        echo "Running perf record for detailed profiling..."
        perf record -g -o "$TEST_OUTPUT_DIR/perf.data" \
            bash -c "$TEST_CMD" \
            > "$TEST_OUTPUT_DIR/perf_record_raw.txt" 2>&1
        
        PERF_RECORD_EXIT=$?
        
        # Filter warnings but keep errors
        grep -vE "(WARNING: perf not found|You may need to install|You may also want to install|linux-tools-|linux-cloud-tools-)" \
            "$TEST_OUTPUT_DIR/perf_record_raw.txt" >> "$TEST_OUTPUT_DIR/test_output.txt" || true
        
        if [ "$PERF_RECORD_EXIT" -eq 0 ]; then
            PERF_RECORD_SUCCESS=true
        else
            PERF_RECORD_SUCCESS=false
            echo "WARNING: perf record exit code: $PERF_RECORD_EXIT"
            echo "Check full output: $TEST_OUTPUT_DIR/perf_record_raw.txt"
        fi
        
        # Generate perf reports
        if [ "${PERF_RECORD_SUCCESS:-false}" = true ] && [ -f "$TEST_OUTPUT_DIR/perf.data" ]; then
            echo ""
            echo "Generating perf reports..."
            perf report -i "$TEST_OUTPUT_DIR/perf.data" \
                --stdio > "$TEST_OUTPUT_DIR/perf_report.txt" 2>&1 || true
            
            perf script -i "$TEST_OUTPUT_DIR/perf.data" > "$TEST_OUTPUT_DIR/perf_script.txt" 2>&1 || true
        fi
    else
        echo "Skipping perf record (test failed)"
    fi
else
    # Run without perf
    echo "Running test (no perf, async-profiler and GC logging only)..."
    bash -c "$TEST_CMD" > "$TEST_OUTPUT_DIR/test_output.txt" 2>&1
    TEST_EXIT_CODE=$?
    
    if [ "$TEST_EXIT_CODE" -ne 0 ]; then
        echo "WARNING: Test exit code: $TEST_EXIT_CODE"
        echo "Check output: $TEST_OUTPUT_DIR/test_output.txt"
    fi
fi

echo ""
echo "=== Step 3: Collecting profiling results ==="
echo ""

# Check what was generated
RESULTS_SUMMARY="$TEST_OUTPUT_DIR/results_summary.txt"
cat > "$RESULTS_SUMMARY" << EOF
Full K* Profiling Results Summary
Generated: $(date)
Test: $FULL_TEST_NAME
Output Directory: $TEST_OUTPUT_DIR
Test Status: $(if [ -f "$TEST_OUTPUT_DIR/test_verify.txt" ] && grep -q "PASSED\|BUILD SUCCESSFUL" "$TEST_OUTPUT_DIR/test_verify.txt"; then echo "PASSED"; else echo "FAILED or UNKNOWN"; fi)

Files Generated:
EOF

if [ -f "$GC_LOG_FILE" ]; then
    echo "GC log: $GC_LOG_FILE" | tee -a "$RESULTS_SUMMARY"
    GC_SIZE=$(du -h "$GC_LOG_FILE" | cut -f1)
    echo "  Size: $GC_SIZE" | tee -a "$RESULTS_SUMMARY"
else
    echo "GC log: NOT FOUND" | tee -a "$RESULTS_SUMMARY"
fi

if [ -f "$TEST_OUTPUT_DIR/async_profiler_cpu.html" ]; then
    echo "Async-profiler CPU: $TEST_OUTPUT_DIR/async_profiler_cpu.html" | tee -a "$RESULTS_SUMMARY"
else
    echo "Async-profiler CPU: NOT FOUND" | tee -a "$RESULTS_SUMMARY"
fi

if [ -f "$TEST_OUTPUT_DIR/perf_stat.txt" ]; then
    echo "Perf stat: $TEST_OUTPUT_DIR/perf_stat.txt" | tee -a "$RESULTS_SUMMARY"
else
    echo "Perf stat: NOT FOUND" | tee -a "$RESULTS_SUMMARY"
fi

if [ -f "$TEST_OUTPUT_DIR/perf.data" ]; then
    echo "Perf data: $TEST_OUTPUT_DIR/perf.data" | tee -a "$RESULTS_SUMMARY"
    PERF_SIZE=$(du -h "$TEST_OUTPUT_DIR/perf.data" | cut -f1)
    echo "  Size: $PERF_SIZE" | tee -a "$RESULTS_SUMMARY"
else
    echo "Perf data: NOT FOUND" | tee -a "$RESULTS_SUMMARY"
fi

if [ -f "$TEST_OUTPUT_DIR/perf_report.txt" ]; then
    echo "Perf report: $TEST_OUTPUT_DIR/perf_report.txt" | tee -a "$RESULTS_SUMMARY"
else
    echo "Perf report: NOT FOUND" | tee -a "$RESULTS_SUMMARY"
fi

echo "" | tee -a "$RESULTS_SUMMARY"
echo "=== Next Steps ===" | tee -a "$RESULTS_SUMMARY"
echo "" | tee -a "$RESULTS_SUMMARY"

if [ -f "$GC_LOG_FILE" ]; then
    echo "1. Analyze GC logs:" | tee -a "$RESULTS_SUMMARY"
    echo "   python3 $SCRIPT_DIR/analyze_pipeline_memory.py --gc-log $GC_LOG_FILE --output-dir $TEST_OUTPUT_DIR/gc_analysis" | tee -a "$RESULTS_SUMMARY"
    echo "" | tee -a "$RESULTS_SUMMARY"
fi

if [ -f "$TEST_OUTPUT_DIR/async_profiler_cpu.html" ]; then
    echo "2. Analyze async-profiler HTML:" | tee -a "$RESULTS_SUMMARY"
    echo "   python3 $SCRIPT_DIR/analyze_async_profiler.py --profile-dir $TEST_OUTPUT_DIR" | tee -a "$RESULTS_SUMMARY"
    echo "" | tee -a "$RESULTS_SUMMARY"
fi

if [ -f "$TEST_OUTPUT_DIR/perf.data" ]; then
    echo "3. Analyze perf data:" | tee -a "$RESULTS_SUMMARY"
    echo "   perf report -i $TEST_OUTPUT_DIR/perf.data" | tee -a "$RESULTS_SUMMARY"
    echo "   python3 $SCRIPT_DIR/analyze_cpu_profile.py --output-dir $TEST_OUTPUT_DIR" | tee -a "$RESULTS_SUMMARY"
    echo "" | tee -a "$RESULTS_SUMMARY"
fi

echo "4. Combined analysis (when tool is ready):" | tee -a "$RESULTS_SUMMARY"
echo "   python3 $SCRIPT_DIR/analyze_combined_profiles.py --output-dir $TEST_OUTPUT_DIR" | tee -a "$RESULTS_SUMMARY"

echo ""
echo "=== Profiling Complete ==="
echo "Results in: $TEST_OUTPUT_DIR"
echo "Summary: $RESULTS_SUMMARY"
echo ""
cat "$RESULTS_SUMMARY"

