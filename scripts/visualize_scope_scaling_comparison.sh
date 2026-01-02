#!/bin/bash
# Run SCOPE with different amino acid set sizes and visualize the difference

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

cd "$REPO_ROOT"

PDB_FILE="${1:-examples/python.KStar/2RL0.min.reduce.pdb}"
OUTPUT_BASE="pdb_hulls_scaling_comparison"

echo "=== SCOPE Scaling Visualization Comparison ==="
echo "PDB file: $PDB_FILE"
echo "Output base: $OUTPUT_BASE"
echo

# Create Python script to run SCOPE with different AA set sizes
cat > /tmp/run_scope_comparison.py << PYTHON_EOF
#!/usr/bin/env python3
"""Run SCOPE with different amino acid set sizes for comparison"""

import sys
import os
from pathlib import Path

# Add CCKStar to path - use absolute path from repo root
repo_root = Path("${REPO_ROOT}")
sys.path.insert(0, str(repo_root / "src/main/python/CCKStar"))
from Find_Doublets import SCOPE
PYTHON_EOF

# Amino acid sets of different sizes
aa_sets = {
    'minimal_3aa': {
        'aas': ['VAL', 'ALA', 'LEU'],
        'description': 'Minimal: 3 amino acids'
    },
    'small_5aa': {
        'aas': ['VAL', 'ALA', 'LEU', 'ILE', 'CYS'],
        'description': 'Small: 5 amino acids'
    },
    'medium_10aa': {
        'aas': ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP', 'PHE', 'LYS', 'ARG', 'ALA'],
        'description': 'Medium: 10 amino acids'
    },
    'full_22aa': {
        'aas': ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP',
                'PHE', 'LYS', 'ARG', 'HID', 'HIE', 'HIP', 'SER', 'THR', 'TYR',
                'ASN', 'GLN', 'ASP', 'GLU', 'ALA', 'GLY', 'PRO'],
        'description': 'Full: 22 amino acids (all)'
    }
}

pdb_file = sys.argv[1]
output_base = sys.argv[2]

print(f"Running SCOPE with different AA set sizes on {pdb_file}")
print()

for name, config in aa_sets.items():
    output_folder = f"{output_base}/{name}"
    os.makedirs(output_folder, exist_ok=True)
    
    print(f"=== {name} ({config['description']}) ===")
    print(f"  Amino acids: {len(config['aas'])} types")
    print(f"  Output: {output_folder}")
    
    try:
        intrachain_pairs, interchain_pairs = SCOPE(
            pdb_file,
            output_folder,
            'G',  # Design chain
            config['aas'],
            True,  # Save PDB hulls
            'L',   # Chirality
            []     # Fixed residues
        )
        
        num_intra = len(intrachain_pairs) if intrachain_pairs else 0
        num_inter = len(interchain_pairs) if interchain_pairs else 0
        
        print(f"  ✓ Success: {num_intra} intra-chain pairs, {num_inter} inter-chain pairs")
    except Exception as e:
        print(f"  ✗ Error: {e}")
        import traceback
        traceback.print_exc()
    
    print()

print("=== Comparison Complete ===")
print(f"Results saved to: {output_base}/")
print()
print("To visualize:")
print(f"  python3 scripts/visualize_scope_hulls.py --hull-folder {output_base}/minimal_3aa --chain G --residue 638")
print(f"  python3 scripts/visualize_scope_hulls.py --hull-folder {output_base}/full_22aa --chain G --residue 638")
PYTHON_EOF

cd "$REPO_ROOT"
python3 /tmp/run_scope_comparison.py "$PDB_FILE" "$OUTPUT_BASE"

echo
echo "=== Generating Comparison Visualizations ==="

# Visualize the same residue (G638) with different AA set sizes
for aa_set in minimal_3aa small_5aa medium_10aa full_22aa; do
    if [ -d "$OUTPUT_BASE/$aa_set" ]; then
        echo "Visualizing $aa_set..."
        python3 scripts/visualize_scope_hulls.py \
            --hull-folder "$OUTPUT_BASE/$aa_set" \
            --chain G \
            --residue 638 \
            --output "$OUTPUT_BASE/${aa_set}_G638.png" 2>/dev/null || \
        python3 -c "
import sys
sys.path.insert(0, 'src/main/python/CCKStar')
from visualize_scope_hulls import visualize_hulls
visualize_hulls('$OUTPUT_BASE/$aa_set', chain='G', residue=638, show_all=False, opacity=0.7)
" || echo "  (Interactive visualization - close window to continue)"
    fi
done

echo
echo "=== Comparison Complete ==="
echo "Hull files: $OUTPUT_BASE/<aa_set>/"
echo
echo "Key differences to observe:"
echo "  - Hull size/complexity increases with more amino acid types"
echo "  - More points per hull = more complex geometry"
echo "  - Larger hulls = more potential intersections"

