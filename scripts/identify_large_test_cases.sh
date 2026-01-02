#!/bin/bash
# Identify large test cases from examples directory
# Finds systems with most atoms/pairs for comprehensive profiling

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
EXAMPLES_DIR="$PROJECT_ROOT/examples"

echo "=== Identifying Large Test Cases ==="
echo "Analyzing examples directory: $EXAMPLES_DIR"
echo ""

# Run Python analysis script
if [ -f "$SCRIPT_DIR/tools/analyze_example_workloads.py" ]; then
    python3 "$SCRIPT_DIR/tools/analyze_example_workloads.py" "$EXAMPLES_DIR"
else
    echo "Analysis script not found. Creating basic analysis..."
    
    # Basic PDB file analysis
    echo "Finding PDB files and counting atoms..."
    find "$EXAMPLES_DIR" -name "*.pdb" -type f | while read pdb_file; do
        atom_count=$(grep -c "^ATOM  \|^HETATM" "$pdb_file" 2>/dev/null || echo "0")
        if [ "$atom_count" -gt 0 ]; then
            echo "$atom_count atoms: $pdb_file"
        fi
    done | sort -rn | head -20
fi

echo ""
echo "=== Next Steps ==="
echo "1. Review the analysis above to identify largest systems"
echo "2. Check if those systems have corresponding test cases in TestKStar.java"
echo "3. Add large test cases to profile_cpu_bottlenecks.sh"
echo "4. Run profiling on both small and large systems for comparison"

