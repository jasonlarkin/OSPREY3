#!/bin/bash

# Test Runner Script - Run Multiple Test Suites
# This script runs various test suites and captures results

cd "$(dirname "$0")"

echo "========================================="
echo "OSPREY3 Test Suite Runner"
echo "========================================="
echo ""

RESULTS_DIR="test-results-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$RESULTS_DIR"

# Test suites to run
declare -a TEST_SUITES=(
    "edu.duke.cs.osprey.gmec.TestFindGMEC"
    "edu.duke.cs.osprey.gmec.TestSimpleGMECFinder"
    "edu.duke.cs.osprey.gmec.TestComets"
    "edu.duke.cs.osprey.gmec.TestDEEGMECFinder"
    "edu.duke.cs.osprey.astar.TestAStar"
    "edu.duke.cs.osprey.TestCOMETS"
    "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator"
    "edu.duke.cs.osprey.energy.compiled.TestEnergyComparison"
    "edu.duke.cs.osprey.ematrix.TestSimpleEnergyCalculator"
    "edu.duke.cs.osprey.ematrix.TestSimpleEnergyMatrixCalculator"
    "edu.duke.cs.osprey.kstar.TestKStar"
    "edu.duke.cs.osprey.kstar.TestBBKStar"
    "edu.duke.cs.osprey.kstar.compiled.TestKStar"
    "edu.duke.cs.osprey.kstar.compiled.TestBBKStar"
    "edu.duke.cs.osprey.confspace.TestSimpleConfSpace"
    "edu.duke.cs.osprey.energy.forcefield.TestForcefieldEnergy"
    "edu.duke.cs.osprey.python.TestPythonScripts"
)

TOTAL=${#TEST_SUITES[@]}
PASSED=0
FAILED=0

echo "Running $TOTAL test suites..."
echo "Results will be saved to: $RESULTS_DIR"
echo ""

for i in "${!TEST_SUITES[@]}"; do
    TEST_SUITE="${TEST_SUITES[$i]}"
    SUITE_NUM=$((i+1))
    
    echo "[$SUITE_NUM/$TOTAL] Running: $TEST_SUITE"
    
    # Clean test suite name for filename
    SUITE_FILE=$(echo "$TEST_SUITE" | tr '.' '_')
    OUTPUT_FILE="$RESULTS_DIR/${SUITE_FILE}.log"
    
    # Run the test suite
    ./gradlew clean test --tests "$TEST_SUITE" > "$OUTPUT_FILE" 2>&1
    EXIT_CODE=$?
    
    # Parse results
    if grep -q "BUILD SUCCESSFUL" "$OUTPUT_FILE"; then
        if grep -q "tests completed" "$OUTPUT_FILE"; then
            TEST_COUNT=$(grep -oP '\d+ tests?' "$OUTPUT_FILE" | head -1 || echo "?")
            echo "  -> PASSED: $TEST_COUNT"
            echo "PASS: $TEST_SUITE" >> "$RESULTS_DIR/summary.txt"
            PASSED=$((PASSED + 1))
        else
            echo "  -> SKIPPED (no tests found)"
            echo "SKIP: $TEST_SUITE" >> "$RESULTS_DIR/summary.txt"
        fi
    else
        echo "  -> FAILED"
        echo "FAIL: $TEST_SUITE" >> "$RESULTS_DIR/summary.txt"
        FAILED=$((FAILED + 1))
    fi
    
    echo ""
done

echo "========================================="
echo "Test Suite Summary"
echo "========================================="
echo "Total: $TOTAL"
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Skipped: $((TOTAL - PASSED - FAILED))"
echo ""
echo "Detailed results in: $RESULTS_DIR/"
echo "Summary: $RESULTS_DIR/summary.txt"
echo ""

# Display summary
if [ -f "$RESULTS_DIR/summary.txt" ]; then
    echo "Quick Summary:"
    echo "-------------"
    cat "$RESULTS_DIR/summary.txt"
fi

