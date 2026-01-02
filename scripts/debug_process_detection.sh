#!/bin/bash
# Debug process detection

echo "=== Testing Process Detection ==="

# Test 1: List all Java processes
echo "1. All Java PIDs:"
pgrep -f "java" | head -5

echo ""
echo "2. Full command lines from /proc:"
for pid in $(pgrep -f "java" | head -5); do
    echo "PID $pid:"
    CMD=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ' || echo "FAILED")
    echo "  $CMD"
    echo ""
done

echo "3. Testing Gradle filter:"
for pid in $(pgrep -f "java" | head -5); do
    CMD=$(cat "/proc/$pid/cmdline" 2>/dev/null | tr '\0' ' ' || echo "")
    if [ -z "$CMD" ]; then
        echo "PID $pid: Empty CMD"
        continue
    fi
    if echo "$CMD" | grep -qE "(gradle.*wrapper|gradle.*daemon|gradlew|GradleMain|Dorg.gradle.appname)"; then
        echo "PID $pid: FILTERED (Gradle)"
    else
        echo "PID $pid: NOT FILTERED - $CMD"
    fi
done

