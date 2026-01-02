#!/bin/bash
# Test SCOPE with varying input sizes to understand scaling behavior

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/scope/scaling_tests"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

cd "$REPO_ROOT"

# Default PDB file
PDB_FILE="${1:-examples/python.KStar/2RL0.min.reduce.pdb}"

echo "=== SCOPE Scaling Tests ==="
echo "PDB file: $PDB_FILE"
echo "Output directory: $OUTPUT_DIR"
echo

mkdir -p "$OUTPUT_DIR"

# Create Python script for scaling tests
cat > "$OUTPUT_DIR/test_scope_scaling.py" << 'PYTHON_EOF'
#!/usr/bin/env python3
"""Test SCOPE with varying input sizes"""

import sys
import time
import os
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent.parent / "src/main/python/CCKStar"))
from Find_Doublets import SCOPE

# All amino acids
ALL_AAS = ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP',
           'PHE', 'LYS', 'ARG', 'HID', 'HIE', 'HIP', 'SER', 'THR', 'TYR',
           'ASN', 'GLN', 'ASP', 'GLU', 'ALA', 'GLY', 'PRO']

# Test configurations
test_configs = [
    {
        'name': 'minimal_3aa',
        'aas': ['VAL', 'ALA', 'LEU'],
        'description': 'Minimal: 3 amino acid types'
    },
    {
        'name': 'small_5aa',
        'aas': ['VAL', 'ALA', 'LEU', 'ILE', 'CYS'],
        'description': 'Small: 5 amino acid types'
    },
    {
        'name': 'medium_10aa',
        'aas': ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP', 'PHE', 'LYS', 'ARG', 'ALA'],
        'description': 'Medium: 10 amino acid types'
    },
    {
        'name': 'large_15aa',
        'aas': ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP', 'PHE', 'LYS', 'ARG', 
                'HID', 'HIE', 'HIP', 'SER', 'THR', 'TYR'],
        'description': 'Large: 15 amino acid types'
    },
    {
        'name': 'full_22aa',
        'aas': ALL_AAS,
        'description': 'Full: 22 amino acid types (all)'
    }
]

def run_scope_test(pdb_file, aas, test_name, output_base):
    """Run SCOPE and measure time"""
    output_folder = f"{output_base}/{test_name}"
    os.makedirs(output_folder, exist_ok=True)
    
    print(f"\n=== {test_name} ===")
    print(f"Description: {test_configs[next(i for i, t in enumerate(test_configs) if t['name'] == test_name)]['description']}")
    print(f"Amino acids: {len(aas)} types")
    print(f"Output: {output_folder}")
    
    start_time = time.time()
    
    try:
        intrachain_pairs, interchain_pairs = SCOPE(
            pdb_file,
            output_folder,
            'G',  # Design chain
            aas,
            True,  # Save PDB hulls
            'L',   # Chirality
            []     # Fixed residues
        )
        
        elapsed = time.time() - start_time
        
        # Count results
        num_intrachain = len(intrachain_pairs) if intrachain_pairs else 0
        num_interchain = len(interchain_pairs) if interchain_pairs else 0
        
        print(f"Time: {elapsed:.2f} seconds")
        print(f"Intra-chain pairs: {num_intrachain}")
        print(f"Inter-chain pairs: {num_interchain}")
        
        return {
            'name': test_name,
            'num_aas': len(aas),
            'time': elapsed,
            'intrachain_pairs': num_intrachain,
            'interchain_pairs': num_interchain,
            'success': True
        }
    except Exception as e:
        elapsed = time.time() - start_time
        print(f"ERROR: {e}")
        import traceback
        traceback.print_exc()
        return {
            'name': test_name,
            'num_aas': len(aas),
            'time': elapsed,
            'success': False,
            'error': str(e)
        }

if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(description='Test SCOPE scaling')
    parser.add_argument('--pdb', default='examples/python.KStar/2RL0.min.reduce.pdb',
                       help='PDB file to use')
    parser.add_argument('--output', default='pipeline_analysis/scope/scaling_tests',
                       help='Output directory')
    parser.add_argument('--test', choices=[t['name'] for t in test_configs] + ['all'],
                       default='all', help='Which test to run')
    
    args = parser.parse_args()
    
    pdb_file = Path(__file__).parent.parent.parent / args.pdb
    if not pdb_file.exists():
        print(f"ERROR: PDB file not found: {pdb_file}")
        sys.exit(1)
    
    output_base = Path(__file__).parent.parent.parent / args.output
    
    results = []
    
    if args.test == 'all':
        tests_to_run = test_configs
    else:
        tests_to_run = [t for t in test_configs if t['name'] == args.test]
    
    print(f"Running {len(tests_to_run)} test(s) on {pdb_file}")
    
    for config in tests_to_run:
        result = run_scope_test(
            str(pdb_file),
            config['aas'],
            config['name'],
            str(output_base)
        )
        results.append(result)
    
    # Print summary
    print("\n" + "="*60)
    print("SCALING TEST SUMMARY")
    print("="*60)
    print(f"{'Test':<20} {'AA Types':<12} {'Time (s)':<12} {'Intra':<8} {'Inter':<8} {'Status':<10}")
    print("-"*60)
    
    for r in results:
        if r['success']:
            print(f"{r['name']:<20} {r['num_aas']:<12} {r['time']:<12.2f} {r['intrachain_pairs']:<8} {r['interchain_pairs']:<8} {'SUCCESS':<10}")
        else:
            print(f"{r['name']:<20} {r['num_aas']:<12} {r['time']:<12.2f} {'-':<8} {'-':<8} {'FAILED':<10}")
    
    # Save results to CSV
    import csv
    csv_file = output_base / "scaling_results.csv"
    with open(csv_file, 'w', newline='') as f:
        writer = csv.writer(f)
        writer.writerow(['Test', 'AA_Types', 'Time_Seconds', 'Intrachain_Pairs', 'Interchain_Pairs', 'Success'])
        for r in results:
            if r['success']:
                writer.writerow([r['name'], r['num_aas'], r['time'], 
                               r['intrachain_pairs'], r['interchain_pairs'], 'SUCCESS'])
            else:
                writer.writerow([r['name'], r['num_aas'], r['time'], '-', '-', 'FAILED'])
    
    print(f"\nResults saved to: {csv_file}")
PYTHON_EOF

chmod +x "$OUTPUT_DIR/test_scope_scaling.py"

echo "Running scaling tests..."
python3 "$OUTPUT_DIR/test_scope_scaling.py" --pdb "$PDB_FILE" --output "$OUTPUT_DIR"

echo
echo "=== Scaling Tests Complete ==="
echo "Results saved to: $OUTPUT_DIR/scaling_results.csv"
echo "Hull files saved to: $OUTPUT_DIR/<test_name>/"

