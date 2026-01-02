#!/bin/bash
# Quick test script for PyVista visualization

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

cd "$REPO_ROOT"

HULL_FOLDER="${1:-pdb_hulls_2rl0}"

echo "=== Testing PyVista Visualization ==="
echo "Hull folder: $HULL_FOLDER"
echo

# Check if hull folder exists
if [ ! -d "$HULL_FOLDER" ]; then
    echo "ERROR: Hull folder not found: $HULL_FOLDER"
    echo "Run SCOPE first to generate hull files"
    exit 1
fi

# Test different visualization modes
echo "Available visualization options:"
echo "1. Show representative hulls (one per chain)"
echo "2. Show specific chain (G - design chain)"
echo "3. Show specific residue (G638)"
echo "4. Show intersections"
echo

# Start with simple representative view
echo "Running: Representative view (one hull per chain)..."
python3 scripts/visualize_scope_hulls.py --hull-folder "$HULL_FOLDER" || echo "Visualization closed"

echo
echo "To try other views, run:"
echo "  python3 scripts/visualize_scope_hulls.py --hull-folder $HULL_FOLDER --chain G"
echo "  python3 scripts/visualize_scope_hulls.py --hull-folder $HULL_FOLDER --chain G --residue 638"
echo "  python3 scripts/visualize_scope_hulls.py --hull-folder $HULL_FOLDER --show-intersections"
echo "  python3 scripts/visualize_scope_hulls.py --hull-folder $HULL_FOLDER --show-all"

