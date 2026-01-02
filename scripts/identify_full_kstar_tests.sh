#!/bin/bash
# Identify full K* test cases (not minimal)
# Lists test methods that run complete K* calculations

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "=== Full K* Test Cases ==="
echo ""
echo "These test methods run complete K* calculations (not minimal):"
echo ""

# TestKStar.java test methods
echo "## TestKStar.java (edu.duke.cs.osprey.kstar.TestKStar)"
echo ""
echo "**Full K* Calculations:**"
echo "  - test2RL0()                    # 2RL0 system: 16,734 pairs (complex), 7,575 pairs (ligand), 1,008 pairs (protein)"
echo "  - test1GUA11()                  # 1GUA11 system: full calculation"
echo ""
echo "**Variants (reduced):**"
echo "  - test2RL0OnlyOneMutant()       # 2RL0 with only one mutant sequence"
echo "  - test2RL0SpaceWithoutWildType() # 2RL0 without wild type"
echo "  - test2RL0WithExternalMemory()   # 2RL0 with external memory (TPIE)"
echo "  - test2RL0WithConfDB()           # 2RL0 with conformation database"
echo ""

# Check if we can get more details from the test file
TEST_FILE="$PROJECT_ROOT/src/test/java/edu/duke/cs/osprey/kstar/TestKStar.java"
if [ -f "$TEST_FILE" ]; then
    echo "## Test File Details"
    echo ""
    echo "Test file: $TEST_FILE"
    echo ""
    
    # Count test methods
    TEST_COUNT=$(grep -c "^[[:space:]]*@Test" "$TEST_FILE" || echo "0")
    echo "Total @Test methods found: $TEST_COUNT"
    echo ""
    
    # Show make methods (conf space builders)
    echo "ConfSpace builder methods:"
    grep "^[[:space:]]*public static.*make" "$TEST_FILE" | sed 's/^[[:space:]]*/  - /' || echo "  (none found)"
    echo ""
fi

# Python examples
echo "## Python Examples"
echo ""
if [ -d "$PROJECT_ROOT/examples/python.KStar" ]; then
    echo "Python K* examples:"
    find "$PROJECT_ROOT/examples/python.KStar" -name "*.py" -type f | while read py_file; do
        echo "  - $(basename "$py_file")"
    done
    echo ""
fi

# Summary
echo "## Recommended Test Cases for Full K* Profiling"
echo ""
echo "**Primary targets (full calculations):**"
echo "  1. test2RL0() - 2RL0 system, largest workload (16,734 pairs in complex)"
echo "  2. test1GUA11() - 1GUA11 system, alternative workload"
echo ""
echo "**Usage:"
echo "  ./gradlew test --tests edu.duke.cs.osprey.kstar.TestKStar.test2RL0"
echo "  ./gradlew test --tests edu.duke.cs.osprey.kstar.TestKStar.test1GUA11"
echo ""

