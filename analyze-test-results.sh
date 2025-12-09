#!/bin/bash

# Analyze Test Results - Get Accurate Pass/Fail Counts

RESULTS_DIR="$1"

if [ -z "$RESULTS_DIR" ]; then
    # Find most recent results directory
    RESULTS_DIR=$(ls -td test-results-*/ 2>/dev/null | head -1)
fi

if [ -z "$RESULTS_DIR" ] || [ ! -d "$RESULTS_DIR" ]; then
    echo "Error: No test results directory found"
    echo "Usage: $0 [results-directory]"
    exit 1
fi

echo "========================================="
echo "Test Results Analysis"
echo "========================================="
echo "Analyzing: $RESULTS_DIR"
echo ""

TOTAL_LOGS=$(ls "$RESULTS_DIR"/*.log 2>/dev/null | wc -l)
echo "Total log files: $TOTAL_LOGS"
echo ""

# Count passing tests (individual PASSED lines)
PASSED_TESTS=$(grep -h " PASSED" "$RESULTS_DIR"/*.log 2>/dev/null | wc -l)
echo "Individual tests PASSED: $PASSED_TESTS"

# Count failing tests (individual FAILED lines)
FAILED_TESTS=$(grep -h " FAILED" "$RESULTS_DIR"/*.log 2>/dev/null | wc -l)
echo "Individual tests FAILED: $FAILED_TESTS"
echo ""

# Test suites that built successfully
echo "Test suites with BUILD SUCCESSFUL:"
SUCCESSFUL_BUILDS=$(grep -l "BUILD SUCCESSFUL" "$RESULTS_DIR"/*.log 2>/dev/null)
SUCCESSFUL_COUNT=$(echo "$SUCCESSFUL_BUILDS" | grep -c . || echo "0")
echo "  Total: $SUCCESSFUL_COUNT"

# Test suites with passing tests
echo ""
echo "Test suites with PASSING tests:"
PASSING_SUITES=$(echo "$SUCCESSFUL_BUILDS" | xargs grep -l " PASSED" 2>/dev/null)
PASSING_COUNT=$(echo "$PASSING_SUITES" | grep -c . || echo "0")
echo "  Total: $PASSING_COUNT"
echo "$PASSING_SUITES" | while read file; do
    if [ -n "$file" ]; then
        SUITE_NAME=$(basename "$file" .log | tr '_' '.')
        TEST_COUNT=$(grep -c " PASSED" "$file" 2>/dev/null || echo "0")
        echo "    - $SUITE_NAME: $TEST_COUNT tests passed"
    fi
done

# Test suites that failed to build
echo ""
echo "Test suites with BUILD FAILED:"
FAILED_BUILDS=$(grep -l "BUILD FAILED" "$RESULTS_DIR"/*.log 2>/dev/null)
FAILED_COUNT=$(echo "$FAILED_BUILDS" | grep -c . || echo "0")
echo "  Total: $FAILED_COUNT"
echo "$FAILED_BUILDS" | while read file; do
    if [ -n "$file" ]; then
        SUITE_NAME=$(basename "$file" .log | tr '_' '.')
        echo "    - $SUITE_NAME"
    fi
done

# Test suites with no test output (skipped?)
echo ""
echo "Test suites with no test output (may be skipped):"
NO_OUTPUT=$(echo "$SUCCESSFUL_BUILDS" | while read file; do
    if [ -n "$file" ]; then
        if ! grep -q " PASSED\| FAILED\|tests completed" "$file" 2>/dev/null; then
            echo "$file"
        fi
    fi
done)
NO_OUTPUT_COUNT=$(echo "$NO_OUTPUT" | grep -c . || echo "0")
echo "  Total: $NO_OUTPUT_COUNT"

# Summary
echo ""
echo "========================================="
echo "Summary"
echo "========================================="
echo "Total test suites: $TOTAL_LOGS"
echo "Builds successful: $SUCCESSFUL_COUNT"
echo "Builds failed: $FAILED_COUNT"
echo ""
echo "Individual tests passed: $PASSED_TESTS"
echo "Individual tests failed: $FAILED_TESTS"
echo ""
echo "Test suites with passing tests: $PASSING_COUNT"
echo "Test suites with failed builds: $FAILED_COUNT"
echo "Test suites with no output: $NO_OUTPUT_COUNT"

