#!/usr/bin/env python3
"""
Compare hull sizes by running SCOPE with different amino acid set sizes
and visualizing the same residue to show scaling differences
"""

import sys
import os
import time
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent.parent / "src/main/python/CCKStar"))
from Find_Doublets import SCOPE
import pyvista as pv
from Find_Doublets import polydata_from_hull, pdb_to_coords
import glob
import re

# Amino acid sets of different sizes
AA_SETS = {
    'minimal_3aa': {
        'aas': ['VAL', 'ALA', 'LEU'],
        'color': 'green',
        'description': 'Minimal: 3 amino acids'
    },
    'small_5aa': {
        'aas': ['VAL', 'ALA', 'LEU', 'ILE', 'CYS'],
        'color': 'cyan',
        'description': 'Small: 5 amino acids'
    },
    'medium_10aa': {
        'aas': ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP', 'PHE', 'LYS', 'ARG', 'ALA'],
        'color': 'orange',
        'description': 'Medium: 10 amino acids'
    },
    'full_22aa': {
        'aas': ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP',
                'PHE', 'LYS', 'ARG', 'HID', 'HIE', 'HIP', 'SER', 'THR', 'TYR',
                'ASN', 'GLN', 'ASP', 'GLU', 'ALA', 'GLY', 'PRO'],
        'color': 'blue',
        'description': 'Full: 22 amino acids (all)'
    }
}

def run_scope(pdb_file, aas, output_folder):
    """Run SCOPE and return results"""
    os.makedirs(output_folder, exist_ok=True)
    
    print(f"Running SCOPE with {len(aas)} amino acid types...")
    start = time.time()
    
    intrachain_pairs, interchain_pairs = SCOPE(
        pdb_file,
        output_folder,
        'G',  # Design chain
        aas,
        True,  # Save PDB hulls
        'L',   # Chirality
        []     # Fixed residues
    )
    
    elapsed = time.time() - start
    num_intra = len(intrachain_pairs) if intrachain_pairs else 0
    num_inter = len(interchain_pairs) if interchain_pairs else 0
    
    print(f"  Time: {elapsed:.2f}s, Intra: {num_intra}, Inter: {num_inter}")
    return elapsed, num_intra, num_inter

def load_hull(pdb_file):
    """Load hull from PDB file"""
    coords = pdb_to_coords(str(pdb_file))
    if len(coords) < 4:
        return None
    return coords

def visualize_comparison(hull_folders, residue=638, output_file=None):
    """Visualize the same residue from different AA set sizes side-by-side"""
    # Use off_screen if saving to file, otherwise interactive
    off_screen = output_file is not None
    plotter = pv.Plotter(shape=(2, 2), off_screen=off_screen)
    plotter.set_background('white')
    
    positions = [
        (0, 0),  # Top left
        (0, 1),  # Top right
        (1, 0),  # Bottom left
        (1, 1)   # Bottom right
    ]
    
    for idx, (name, folder) in enumerate(hull_folders.items()):
        if idx >= 4:
            break
        
        row, col = positions[idx]
        plotter.subplot(row, col)
        
        # Find hull file for this residue
        pattern = str(Path(folder) / f"ChainGRes{residue}.pdb")
        hull_files = glob.glob(pattern)
        
        if not hull_files:
            plotter.add_text(f"{name}\n(No hull found)", font_size=12)
            continue
        
        hull_file = hull_files[0]
        coords = load_hull(hull_file)
        
        if coords is None:
            plotter.add_text(f"{name}\n(Invalid hull)", font_size=12)
            continue
        
        try:
            mesh = polydata_from_hull(coords)
            config = AA_SETS.get(name, {})
            color = config.get('color', 'gray')
            description = config.get('description', name)
            
            plotter.add_mesh(mesh, color=color, opacity=0.7,
                           label=f"{description}\n{len(coords)} points")
            plotter.add_text(f"{description}\n{len(coords)} points", 
                           position='upper_left', font_size=10)
            plotter.camera_position = 'iso'
        except Exception as e:
            plotter.add_text(f"{name}\nError: {str(e)[:30]}", font_size=10)
    
    if output_file:
        plotter.screenshot(output_file)
        print(f"Saved comparison to: {output_file}")
        plotter.close()
    else:
        plotter.show()

def main():
    import argparse
    
    parser = argparse.ArgumentParser(description='Compare SCOPE hull sizes')
    parser.add_argument('--pdb', default='examples/python.KStar/2RL0.min.reduce.pdb',
                       help='PDB file to use')
    parser.add_argument('--residue', type=int, default=638,
                       help='Residue to compare (default: 638)')
    parser.add_argument('--output-base', default='pdb_hulls_scaling_comparison',
                       help='Base output directory')
    parser.add_argument('--skip-scope', action='store_true',
                       help='Skip SCOPE runs, use existing hulls')
    parser.add_argument('--visualize-only', action='store_true',
                       help='Only visualize, do not run SCOPE')
    
    args = parser.parse_args()
    
    pdb_file = Path(__file__).parent.parent / args.pdb
    if not pdb_file.exists():
        print(f"ERROR: PDB file not found: {pdb_file}")
        sys.exit(1)
    
    output_base = Path(__file__).parent.parent / args.output_base
    
    hull_folders = {}
    results = {}
    
    if not args.visualize_only:
        print("=== Running SCOPE with Different AA Set Sizes ===")
        print(f"PDB: {pdb_file}")
        print(f"Residue: G{args.residue}")
        print()
        
        for name, config in AA_SETS.items():
            output_folder = output_base / name
            print(f"\n{name} ({config['description']}):")
            
            if args.skip_scope and output_folder.exists():
                print(f"  Using existing hulls in {output_folder}")
            else:
                elapsed, num_intra, num_inter = run_scope(
                    str(pdb_file),
                    config['aas'],
                    str(output_folder)
                )
                results[name] = {
                    'time': elapsed,
                    'intra': num_intra,
                    'inter': num_inter,
                    'num_aas': len(config['aas'])
                }
            
            hull_folders[name] = str(output_folder)
    
    else:
        # Just use existing folders
        for name in AA_SETS.keys():
            folder = output_base / name
            if folder.exists():
                hull_folders[name] = str(folder)
    
    if results:
        print("\n=== Performance Summary ===")
        print(f"{'AA Set':<20} {'AA Types':<12} {'Time (s)':<12} {'Intra':<8} {'Inter':<8}")
        print("-" * 60)
        for name, r in results.items():
            print(f"{name:<20} {r['num_aas']:<12} {r['time']:<12.2f} {r['intra']:<8} {r['inter']:<8}")
    
    print("\n=== Generating Comparison Visualization ===")
    output_file = output_base / f"comparison_residue_{args.residue}.png"
    visualize_comparison(hull_folders, residue=args.residue, output_file=str(output_file))
    
    print(f"\n=== Complete ===")
    print(f"Comparison saved to: {output_file}")
    print(f"Hull folders: {output_base}/")

if __name__ == '__main__':
    main()

