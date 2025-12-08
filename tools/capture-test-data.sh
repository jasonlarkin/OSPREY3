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
    # Use standalone main class - compile first, then run with java
    ./gradlew compileJava compileKotlin || true
    # Run with java using the build classpath
    java -cp "build/classes/java/main:build/classes/kotlin/main:build/resources/main:$(./gradlew printClasspath -q 2>/dev/null | tail -1)" \
         --add-modules=jdk.incubator.foreign \
         edu.duke.cs.osprey.tools.GenerateTestData || true
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

