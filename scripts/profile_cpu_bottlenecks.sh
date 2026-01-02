#!/bin/bash
# Profile CPU bottlenecks in OSPREY K* execution
# Uses perf to identify where time is actually spent (not just GC)
# Designed for WSL/Linux environment

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/pipeline_analysis/cpu"
ANALYSIS_SCRIPT="$SCRIPT_DIR/analyze_cpu_profile.py"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=== CPU Bottleneck Profiling ==="
echo "Project root: $PROJECT_ROOT"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Check for perf (works even with WSL kernel warnings)
if command -v perf &> /dev/null; then
    USE_PERF=true
    # Suppress perf kernel warnings by redirecting stderr for version check
    perf --version 2>/dev/null || true
else
    USE_PERF=false
    echo "NOTE: perf not found. Using async-profiler only."
fi

# Check if we're in the right directory
if [ ! -f "$PROJECT_ROOT/gradlew" ]; then
    echo "ERROR: gradlew not found in $PROJECT_ROOT"
    exit 1
fi

cd "$PROJECT_ROOT" || exit 1
chmod +x gradlew 2>/dev/null || true

# Test cases to profile
# Format: "type:name:command:description"
# type: java_test, python_example
# Note: To add larger systems (DAGK 5,658 atoms, EWAKStar up to 5,305 atoms),
#       add python_example entries pointing to example scripts
TEST_CASES=(
    "java_test:test2RL0:edu.duke.cs.osprey.kstar.TestKStar.test2RL0:Standard 2RL0 (larger system)"
    "java_test:test2RL0OnlyOneMutant:edu.duke.cs.osprey.kstar.TestKStar.test2RL0OnlyOneMutant:2RL0 single mutation"
    "java_test:test1GUA11:edu.duke.cs.osprey.kstar.TestKStar.test1GUA11:1GUA system (3,469 atoms)"
)

# Check for minimal mode
MINIMAL_MODE="${1:-}"
if [ "$MINIMAL_MODE" = "--minimal" ]; then
    echo "=== MINIMAL MODE: Using fastest test case ==="
    TEST_CASES=("java_test:test2RL0OnlyOneMutant:edu.duke.cs.osprey.kstar.TestKStar.test2RL0OnlyOneMutant:2RL0 single mutation")
fi

# Profile each test case
for test_case in "${TEST_CASES[@]}"; do
    IFS=':' read -r test_type test_name test_command description <<< "$test_case"
    
    echo "=========================================="
    echo "Profiling: $test_name"
    echo "Type: $test_type"
    echo "Description: $description"
    echo "=========================================="
    
    # Create output directory for this test case
    case_output_dir="$OUTPUT_DIR/$test_name"
    mkdir -p "$case_output_dir"
    
    # Clear any existing JAVA_TOOL_OPTIONS to avoid conflicts
    unset JAVA_TOOL_OPTIONS
    
    # Determine command to run
    if [ "$test_type" = "java_test" ]; then
        RUN_CMD="./gradlew test --tests \"$test_command\" --no-daemon"
        # Look for the actual test JVM process (not Gradle wrapper)
        # Test JVM typically has the test class name in the command line
        JAVA_PATTERN="TestKStar|junit|test"
    elif [ "$test_type" = "python_example" ]; then
        RUN_CMD="python3 $test_command"
        JAVA_PATTERN="python.*osprey"
    else
        echo "Unknown test type: $test_type"
        continue
    fi
    
    # Run with perf stat if available (suppress kernel warnings)
    if [ "$USE_PERF" = true ]; then
        echo "Running perf stat..."
        # Suppress perf kernel warnings (they're informational, perf still works)
        # Redirect perf warnings to /dev/null, keep actual output
        eval "perf stat -o \"$case_output_dir/perf_stat.txt\" \
            -e cycles,instructions,cache-references,cache-misses,branch-instructions,branch-misses,page-faults \
            $RUN_CMD" 2>&1 | grep -vE "(WARNING: perf not found|You may need to install|You may also want to install|linux-tools-|linux-cloud-tools-)" | tee "$case_output_dir/test_output.txt" || true
    fi
    
    # Generate reports
    if [ -f "$case_output_dir/perf.data" ]; then
        echo "Generating perf reports..."
        
        # Top functions
        perf report -i "$case_output_dir/perf.data" \
            --stdio > "$case_output_dir/perf_report.txt" 2>&1 || true
        
        # Flame graph data (if available)
        if command -v perf script &> /dev/null; then
            perf script -i "$case_output_dir/perf.data" > "$case_output_dir/perf_script.txt" 2>&1 || true
        fi
        
        # Annotate specific functions (if we can identify them)
        echo "Generating annotated assembly (if available)..."
        perf annotate -i "$case_output_dir/perf.data" \
            --stdio > "$case_output_dir/perf_annotate.txt" 2>&1 || true
        
        echo "✓ Perf data collected"
    else
        echo "⚠ No perf.data generated"
    fi
    
    # Use async-profiler for Java (primary method, works better than perf for JVM)
    # Try to find profiler.sh wrapper script (preferred) or JAR
    ASYNC_PROFILER=""
    USE_WRAPPER=false
    # First try wrapper script (preferred method)
    for path in "$HOME/async-profiler/profiler.sh" \
                "/home/$USER/async-profiler/profiler.sh"; do
        if [ -f "$path" ] && [ -x "$path" ]; then
            ASYNC_PROFILER="$path"
            USE_WRAPPER=true
            break
        fi
    done
    # If wrapper not found, try JAR locations
    if [ -z "$ASYNC_PROFILER" ]; then
        for path in "$HOME/async-profiler/async-profiler.jar" \
                    "$HOME/async-profiler/build/async-profiler.jar" \
                    "/home/$USER/async-profiler/async-profiler.jar" \
                    "/home/$USER/async-profiler/build/async-profiler.jar"; do
            if [ -f "$path" ]; then
                ASYNC_PROFILER="$path"
                break
            fi
        done
    fi
    
    if [ -n "$ASYNC_PROFILER" ] && [ -f "$ASYNC_PROFILER" ] && command -v java &> /dev/null; then
        echo "Profiling with async-profiler..."
        
        # Find libasyncProfiler.so for agent mode
        ASYNC_PROFILER_DIR="$(dirname "$ASYNC_PROFILER")"
        ASYNC_PROFILER_LIB=""
        for lib_path in "$ASYNC_PROFILER_DIR/libasyncProfiler.so" \
                        "$ASYNC_PROFILER_DIR/build/libasyncProfiler.so" \
                        "$(dirname "$ASYNC_PROFILER_DIR")/libasyncProfiler.so"; do
            if [ -f "$lib_path" ]; then
                ASYNC_PROFILER_LIB="$lib_path"
                break
            fi
        done
        
        # Run test with CPU profiling (profile until process exits)
        echo "CPU profiling run (profiling until test completes)..."
        eval "$RUN_CMD > \"$case_output_dir/test_output.txt\" 2>&1 &"
        TEST_PID=$!
        
        # Wait for JVM to start and attach immediately
        # Give Gradle time to fork the test JVM
        echo "Waiting for test JVM to start..."
        sleep 8
        JAVA_PID=""
        
        # Try multiple patterns to find the actual test JVM
        for i in {1..30}; do
            
            # Look for any Java process (we'll filter out Gradle wrapper later)
            ALL_JAVA_PIDS=$(pgrep -f "java" 2>/dev/null | head -10)
            for pid in $ALL_JAVA_PIDS; do
                # Get FULL command line from /proc (no truncation) or ps
                CMD=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ' || ps -p "$pid" -o args= 2>/dev/null || echo "")
                if [ -z "$CMD" ]; then
                    continue
                fi
                # Skip Gradle wrapper/daemon processes (not just classpath references)
                if echo "$CMD" | grep -qE "(-Dorg.gradle.appname|gradlew|GradleMain)"; then
                    continue
                fi
                # Found a Java process that's not Gradle - this is likely the test JVM
                # Check if it has test-related arguments (junit, test, or our test class)
                if echo "$CMD" | grep -qE "(junit|test|TestKStar|$JAVA_PATTERN)"; then
                    JAVA_PID="$pid"
                    break 2
                fi
            done
            
            sleep 0.5
        done
        
        # If still not found, just take the first non-Gradle Java process
        if [ -z "$JAVA_PID" ]; then
            ALL_JAVA_PIDS=$(pgrep -f "java" 2>/dev/null | head -10)
            for pid in $ALL_JAVA_PIDS; do
                CMD=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ' || ps -p "$pid" -o args= 2>/dev/null || echo "")
                if [ -z "$CMD" ]; then
                    continue
                fi
                # Skip Gradle wrapper/daemon processes (not just classpath references)
                if echo "$CMD" | grep -qE "(-Dorg.gradle.appname|gradlew|GradleMain)"; then
                    continue
                fi
                # This is likely the test JVM
                JAVA_PID="$pid"
                break
            done
        fi
        
        if [ -n "$JAVA_PID" ]; then
            echo "Profiling CPU on PID $JAVA_PID"
            # Use a long duration (600s) to ensure we capture the full test execution
            # The profiler will stop when the process exits, so this is just a safety limit
            if [ "$USE_WRAPPER" = true ]; then
                "$ASYNC_PROFILER" -e cpu -d 600 -f "$case_output_dir/cpu_profile.html" "$JAVA_PID" 2>&1 | tee "$case_output_dir/async_profiler_cpu.log" &
                PROFILER_PID=$!
            else
                java -jar "$ASYNC_PROFILER" -e cpu -d 600 -f "$case_output_dir/cpu_profile.html" "$JAVA_PID" 2>&1 | tee "$case_output_dir/async_profiler_cpu.log" &
                PROFILER_PID=$!
            fi
            
            # Wait for test to complete
            wait $TEST_PID 2>/dev/null || true
            # Give profiler a moment to save
            sleep 2
            # Kill profiler if still running
            kill $PROFILER_PID 2>/dev/null || true
        else
            echo "Could not find Java process - running test without profiling"
            wait $TEST_PID 2>/dev/null || true
        fi
        
        # Run test again with allocation profiling
        echo "Allocation profiling run (profiling until test completes)..."
        eval "$RUN_CMD >> \"$case_output_dir/test_output.txt\" 2>&1 &"
        TEST_PID=$!
        
        echo "Waiting for test JVM to start..."
        sleep 8
        JAVA_PID=""
        
        # Try multiple patterns to find the actual test JVM
        for i in {1..30}; do
            
            # Look for any Java process (we'll filter out Gradle wrapper later)
            ALL_JAVA_PIDS=$(pgrep -f "java" 2>/dev/null | head -10)
            for pid in $ALL_JAVA_PIDS; do
                # Get FULL command line from /proc (no truncation) or ps
                CMD=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ' || ps -p "$pid" -o args= 2>/dev/null || echo "")
                if [ -z "$CMD" ]; then
                    continue
                fi
                # Skip Gradle wrapper/daemon processes (not just classpath references)
                if echo "$CMD" | grep -qE "(-Dorg.gradle.appname|gradlew|GradleMain)"; then
                    continue
                fi
                # Found a Java process that's not Gradle - this is likely the test JVM
                # Check if it has test-related arguments (junit, test, or our test class)
                if echo "$CMD" | grep -qE "(junit|test|TestKStar|$JAVA_PATTERN)"; then
                    JAVA_PID="$pid"
                    break 2
                fi
            done
            
            sleep 0.5
        done
        
        # If still not found, just take the first non-Gradle Java process
        if [ -z "$JAVA_PID" ]; then
            ALL_JAVA_PIDS=$(pgrep -f "java" 2>/dev/null | head -10)
            for pid in $ALL_JAVA_PIDS; do
                CMD=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ' || ps -p "$pid" -o args= 2>/dev/null || echo "")
                if [ -z "$CMD" ]; then
                    continue
                fi
                # Skip Gradle wrapper/daemon processes (not just classpath references)
                if echo "$CMD" | grep -qE "(-Dorg.gradle.appname|gradlew|GradleMain)"; then
                    continue
                fi
                # This is likely the test JVM
                JAVA_PID="$pid"
                break
            done
        fi
        
        if [ -n "$JAVA_PID" ]; then
            echo "Profiling allocations on PID $JAVA_PID"
            # Use a long duration (600s) to ensure we capture the full test execution
            if [ "$USE_WRAPPER" = true ]; then
                "$ASYNC_PROFILER" -e alloc -d 600 -f "$case_output_dir/alloc_profile.html" "$JAVA_PID" 2>&1 | tee "$case_output_dir/async_profiler_alloc.log" &
                PROFILER_PID=$!
            else
                java -jar "$ASYNC_PROFILER" -e alloc -d 600 -f "$case_output_dir/alloc_profile.html" "$JAVA_PID" 2>&1 | tee "$case_output_dir/async_profiler_alloc.log" &
                PROFILER_PID=$!
            fi
            
            # Wait for test to complete
            wait $TEST_PID 2>/dev/null || true
            # Give profiler a moment to save (it stops when process exits)
            sleep 3
            # Kill profiler if still running
            kill $PROFILER_PID 2>/dev/null || true
        else
            echo "Could not find Java process - running test without profiling"
            wait $TEST_PID 2>/dev/null || true
        fi
        
        if [ -f "$case_output_dir/cpu_profile.html" ] || [ -f "$case_output_dir/alloc_profile.html" ]; then
            echo "Profiles generated:"
            [ -f "$case_output_dir/cpu_profile.html" ] && echo "  - CPU: $case_output_dir/cpu_profile.html"
            [ -f "$case_output_dir/alloc_profile.html" ] && echo "  - Allocations: $case_output_dir/alloc_profile.html"
        fi
    else
        echo "async-profiler not found. Tried:"
        echo "  - $HOME/async-profiler/profiler.sh (wrapper script)"
        echo "  - $HOME/async-profiler/async-profiler.jar"
        echo "  - $HOME/async-profiler/build/async-profiler.jar"
        echo "Install with: ./scripts/setup_profiling_tools.sh"
        echo "Running test without profiling..."
        eval "$RUN_CMD" 2>&1 | tee "$case_output_dir/test_output.txt" || true
    fi
    
    echo ""
done

# Generate summary analysis
echo "=========================================="
echo "Generating Summary Analysis"
echo "=========================================="

if [ -f "$ANALYSIS_SCRIPT" ]; then
    python3 "$ANALYSIS_SCRIPT" --output-dir "$OUTPUT_DIR" || echo "⚠ Analysis script failed"
else
    echo "⚠ Analysis script not found: $ANALYSIS_SCRIPT"
    echo "   Creating basic summary..."
    
    # Basic summary
    summary_file="$OUTPUT_DIR/summary.md"
    cat > "$summary_file" << EOF
# CPU Profiling Summary

## Test Cases Profiled

EOF
    
    for test_case in "${TEST_CASES[@]}"; do
        IFS=':' read -r test_type test_name test_command description <<< "$test_case"
        case_output_dir="$OUTPUT_DIR/$test_name"
        
        echo "### $test_name ($test_type)" >> "$summary_file"
        echo "" >> "$summary_file"
        echo "- **Description**: $description" >> "$summary_file"
        
        if [ -f "$case_output_dir/perf_stat.txt" ]; then
            echo "- **Perf stat**: Available" >> "$summary_file"
        fi
        
        if [ -f "$case_output_dir/perf_report.txt" ]; then
            echo "- **Perf report**: Available" >> "$summary_file"
        fi
        
        if [ -f "$case_output_dir/cpu_profile.html" ]; then
            echo "- **CPU Profile (async-profiler)**: Available - Open in browser" >> "$summary_file"
        fi
        
        if [ -f "$case_output_dir/alloc_profile.html" ]; then
            echo "- **Allocation Profile (async-profiler)**: Available - Open in browser" >> "$summary_file"
        fi
        
        echo "" >> "$summary_file"
    done
    
    echo "" >> "$summary_file"
    echo "## Next Steps" >> "$summary_file"
    echo "" >> "$summary_file"
    echo "### View Async-Profiler Flame Graphs" >> "$summary_file"
    echo "" >> "$summary_file"
    echo "Open the HTML files in a web browser to view interactive flame graphs:" >> "$summary_file"
    echo "- CPU profiles: \`cpu_profile.html\` - Shows where CPU time is spent" >> "$summary_file"
    echo "- Allocation profiles: \`alloc_profile.html\` - Shows memory allocation patterns" >> "$summary_file"
    echo "" >> "$summary_file"
    echo "### Review Perf Data (if available)" >> "$summary_file"
    echo "" >> "$summary_file"
    echo "1. Review perf reports: \`cat \$OUTPUT_DIR/*/perf_report.txt\`" >> "$summary_file"
    echo "2. Check perf stat: \`cat \$OUTPUT_DIR/*/perf_stat.txt\`" >> "$summary_file"
    echo "" >> "$summary_file"
    echo "### Look for Hotspots" >> "$summary_file"
    echo "" >> "$summary_file"
    echo "Focus on these areas in the flame graphs:" >> "$summary_file"
    echo "- Energy calculation (C++ code)" >> "$summary_file"
    echo "  - A* search tree traversal" >> "$summary_file"
    echo "  - Partition function computation" >> "$summary_file"
    echo "  - Memory allocation (malloc/new)" >> "$summary_file"
    echo "  - GC pressure (if visible in allocation profile)" >> "$summary_file"
    
    echo "Summary saved to: $summary_file"
fi

echo ""
echo "=== Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR"
echo ""
echo "Next steps:"
echo "1. View async-profiler flame graphs (open in browser):"
echo "   - CPU profiles: $OUTPUT_DIR/*/cpu_profile.html"
echo "   - Allocation profiles: $OUTPUT_DIR/*/alloc_profile.html"
echo ""
echo "2. Review perf reports (if available):"
echo "   - Reports: $OUTPUT_DIR/*/perf_report.txt"
echo "   - Statistics: $OUTPUT_DIR/*/perf_stat.txt"
echo ""
echo "3. Analyze hotspots and bottlenecks from the flame graphs"
echo ""
echo "To view async-profiler profiles:"
echo "  Open in browser: file://$OUTPUT_DIR/<test_method>/cpu_profile.html"
echo "  Open in browser: file://$OUTPUT_DIR/<test_method>/alloc_profile.html"

