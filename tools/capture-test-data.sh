#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR/.."

# Default: fast synthetic test data
MODE="${1:-synthetic}"

echo "Setting up test data directories..."
mkdir -p test-data/serialized
mkdir -p test-data/expected
mkdir -p test-data/metadata

if [ "$MODE" = "synthetic" ]; then
    echo "Capturing synthetic test objects (fast)..."
    # Use test framework approach but with a simple test that just generates data
    # This is more reliable than trying to run main class directly
    ./gradlew test --tests "edu.duke.cs.osprey.tools.CaptureTestObjects" 2>&1 || {
        echo "Test framework approach failed, trying direct Java execution..."
        # Fallback: compile and run directly
        ./gradlew compileJava compileKotlin 2>&1
        # Try to get classpath from Gradle
        CLASSPATH=$(./gradlew -q printClasspath 2>/dev/null || echo "")
        if [ -z "$CLASSPATH" ]; then
            # Build classpath manually
            CLASSPATH="build/classes/java/main:build/classes/kotlin/main:build/resources/main"
            for jar in lib/*.jar; do
                [ -f "$jar" ] && CLASSPATH="$CLASSPATH:$jar"
            done
        fi
        java -cp "$CLASSPATH" \
             --add-modules=jdk.incubator.foreign \
             edu.duke.cs.osprey.tools.GenerateTestData 2>&1 || echo "Direct execution also failed"
    }
elif [ "$MODE" = "osprey" ]; then
    echo "Capturing OSPREY test objects (requires full test framework)..."
    # Use test framework to capture objects from real OSPREY tests
    # This can capture objects from TestFindGMEC, etc.
    ./gradlew test --tests "edu.duke.cs.osprey.tools.CaptureTestObjects" || true
else
    echo "Usage: $0 [synthetic|osprey]"
    echo "  synthetic: Fast generation of simple test objects (default)"
    echo "  osprey:   Capture objects from real OSPREY tests (slower, requires full build)"
    exit 1
fi

echo ""
echo "Captured files:"
ls -lh test-data/serialized/ 2>/dev/null || echo "No serialized files yet"
ls -lh test-data/metadata/ 2>/dev/null || echo "No metadata files yet"

echo ""
echo "Test data capture complete."
echo "Files saved to: test-data/"

