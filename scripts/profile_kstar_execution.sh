#!/bin/bash
# Profile actual K* algorithm execution with memory profiling
# Runs real OSPREY K* code and captures memory patterns
# Designed for WSL/Linux environment

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/pipeline_analysis/memory"
EXAMPLE_DIR="$PROJECT_ROOT/examples/python.KStar"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=== Profiling K* Algorithm Execution ==="
echo "Project root: $PROJECT_ROOT"
echo "Output directory: $OUTPUT_DIR"

# Check if example exists
if [ ! -f "$EXAMPLE_DIR/kstar.py" ]; then
    echo "ERROR: K* example not found at $EXAMPLE_DIR/kstar.py"
    exit 1
fi

# Check if we're in the right directory (should have gradlew)
if [ ! -f "$PROJECT_ROOT/gradlew" ]; then
    echo "WARNING: gradlew not found in $PROJECT_ROOT"
    echo "Make sure you're running from the OSPREY project root"
fi

# Check for minimal mode
MINIMAL_MODE="${1:-}"
if [ "$MINIMAL_MODE" = "--minimal" ]; then
    echo "=== MINIMAL MODE: Using reduced test cases ==="
    MINIMAL_DIR="$PROJECT_ROOT/minimal_test_cases"
    if [ -f "$MINIMAL_DIR/kstar/test_kstar_python_minimal.py" ]; then
        echo "Using minimal Python K* test"
        KSTAR_SCRIPT="$MINIMAL_DIR/kstar/test_kstar_python_minimal.py"
    else
        echo "Minimal test not found, using standard example"
        KSTAR_SCRIPT="$EXAMPLE_DIR/kstar.py"
    fi
else
    KSTAR_SCRIPT="$EXAMPLE_DIR/kstar.py"
fi

# Option 1: Profile Python K* example
echo ""
echo "=== Option 1: Profile Python K* Example ==="
echo "Running: python3 $KSTAR_SCRIPT"

# Change to appropriate directory
if [ "$MINIMAL_MODE" = "--minimal" ]; then
    cd "$PROJECT_ROOT/examples/python.KStar" || exit 1
    PYTHON_CMD="python3 $KSTAR_SCRIPT"
else
    cd "$EXAMPLE_DIR" || exit 1
    PYTHON_CMD="python3 kstar.py"
fi

# NOTE: Cannot use valgrind with Python+Java (JPype loads JVM, valgrind causes JVM crashes)
# Use JVM-native profiling tools instead
echo "NOTE: Python+Java code uses JPype which loads JVM"
echo "      Valgrind is incompatible with JVM - using JVM-native profiling instead"

# Set JVM profiling flags for JPype (Java 17 uses unified logging)
export JAVA_TOOL_OPTIONS="-Xmx2g -XX:+UseG1GC -Xlog:gc*:file=$OUTPUT_DIR/kstar_python_gc.log:time,tags:filecount=5,filesize=10M"

# Run without valgrind (JVM-native profiling via JAVA_TOOL_OPTIONS)
echo "Running with JVM GC logging enabled..."
echo "GC log: $OUTPUT_DIR/kstar_python_gc.log"
$PYTHON_CMD 2>&1 | tee "$OUTPUT_DIR/kstar_python_output.txt"

# Check if it succeeded
if [ ${PIPESTATUS[0]} -eq 0 ]; then
    echo "✓ Test completed successfully"
    if [ -f "$OUTPUT_DIR/kstar_python_gc.log" ]; then
        echo "✓ GC log generated: $OUTPUT_DIR/kstar_python_gc.log"
        echo "  Analyze with: python3 $SCRIPT_DIR/analyze_pipeline_memory.py --gc-log $OUTPUT_DIR/kstar_python_gc.log"
    fi
else
    echo "✗ Test failed - check output: $OUTPUT_DIR/kstar_python_output.txt"
    exit 1
fi

# Return to project root
cd "$PROJECT_ROOT" || exit 1

# Option 2: Profile Java K* test
echo ""
echo "=== Option 2: Profile Java K* Test ==="
echo "Running: ./gradlew test --tests TestKStar"

# Check if gradlew exists
if [ -f "$PROJECT_ROOT/gradlew" ]; then
    cd "$PROJECT_ROOT" || exit 1
    
    # Make gradlew executable if needed
    chmod +x gradlew 2>/dev/null || true
    
    # Run with JVM memory profiling flags (no valgrind - incompatible with JVM)
    echo "Running Java test with GC logging..."
    echo "NOTE: Valgrind cannot be used with JVM (causes crashes)"
    echo "      Using JVM-native GC logging instead"
    
    # Clear JAVA_TOOL_OPTIONS from Python test to avoid conflicts
    unset JAVA_TOOL_OPTIONS
    export GRADLE_OPTS="-Xmx2g -XX:+UseG1GC -Xlog:gc*:file=$OUTPUT_DIR/kstar_java_gc.log:time,tags:filecount=5,filesize=10M"
    
    ./gradlew test --tests "edu.duke.cs.osprey.kstar.TestKStar" \
        --no-daemon 2>&1 | tee "$OUTPUT_DIR/kstar_java_output.txt"
    
    if [ ${PIPESTATUS[0]} -eq 0 ]; then
        echo "✓ Java test completed"
        if [ -f "$OUTPUT_DIR/kstar_java_gc.log" ]; then
            echo "✓ GC log: $OUTPUT_DIR/kstar_java_gc.log"
        fi
    else
        echo "⚠ Java test failed (check test output), but GC log may still be useful"
        if [ -f "$OUTPUT_DIR/kstar_java_gc.log" ]; then
            echo "✓ GC log still generated: $OUTPUT_DIR/kstar_java_gc.log"
        fi
    fi
else
    echo "gradlew not found, skipping Java test"
fi

# Option 3: Profile specific K* test method (faster)
echo ""
echo "=== Option 3: Profile Fast K* Test (if available) ==="

# Look for a fast/small K* test
if [ -f "$PROJECT_ROOT/gradlew" ]; then
    cd "$PROJECT_ROOT" || exit 1
    
    # Make gradlew executable if needed
    chmod +x gradlew 2>/dev/null || true
    
    # Try to find a smaller test
    echo "Looking for fast K* test methods..."
    
    # Run with GC logging to capture forced GC events
    export GRADLE_OPTS="-Xmx2g -XX:+UseG1GC -Xlog:gc*:file=$OUTPUT_DIR/kstar_fast_gc.log:time,tags:filecount=5,filesize=10M"
    
    ./gradlew test --tests "edu.duke.cs.osprey.kstar.TestKStar" \
        --no-daemon 2>&1 | tee "$OUTPUT_DIR/kstar_fast_output.txt" || true
fi

echo ""
echo "=== Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR"
echo ""

# Check what was generated
echo ""
echo "=== Generated Files ==="
if [ -f "$OUTPUT_DIR/kstar_python_gc.log" ]; then
    echo "✓ Python GC log: $OUTPUT_DIR/kstar_python_gc.log"
fi
if [ -f "$OUTPUT_DIR/kstar_java_gc.log" ]; then
    echo "✓ Java GC log: $OUTPUT_DIR/kstar_java_gc.log"
fi
if [ -f "$OUTPUT_DIR/kstar_fast_gc.log" ]; then
    echo "✓ Fast test GC log: $OUTPUT_DIR/kstar_fast_gc.log"
fi

echo ""
echo "=== Next Steps ==="
echo "1. Analyze GC logs:"
if [ -f "$OUTPUT_DIR/kstar_python_gc.log" ]; then
    echo "   python3 $SCRIPT_DIR/analyze_pipeline_memory.py \\"
    echo "       --gc-log $OUTPUT_DIR/kstar_python_gc.log \\"
    echo "       --stage kstar \\"
    echo "       --output-dir $OUTPUT_DIR"
fi
echo ""
echo "2. For detailed allocation profiling, use async-profiler:"
echo "   # Install: wget https://github.com/async-profiler/async-profiler/releases/download/v2.9/async-profiler-2.9-linux-x64.tar.gz"
echo "   # Run: java -jar async-profiler.jar -e alloc -d 60 -f alloc.html <pid>"
echo ""
echo "3. For JVM Flight Recorder (JFR):"
echo "   # Add to JAVA_TOOL_OPTIONS: -XX:+FlightRecorder -XX:StartFlightRecording=duration=60s,filename=osprey.jfr"

