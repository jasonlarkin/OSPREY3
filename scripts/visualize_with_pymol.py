#!/usr/bin/env python3
"""
Visualize OSPREY structures and results using PyMOL

Usage:
    python3 visualize_with_pymol.py --pdb examples/python.KStar/2RL0.min.reduce.pdb
    python3 visualize_with_pymol.py --pdb examples/python.KStar/2RL0.min.reduce.pdb --show-hulls pdb_hulls_2rl0
    python3 visualize_with_pymol.py --pdb examples/python.KStar/2RL0.min.reduce.pdb --design-chain G --target-chain A
"""

import argparse
import sys
from pathlib import Path

try:
    import pymol
    from pymol import cmd
    PYMOl_AVAILABLE = True
except ImportError:
    PYMOl_AVAILABLE = False
    print("ERROR: PyMOL not available. Install with: uv pip install pymol-open-source")
    sys.exit(1)


def visualize_structure(pdb_file, design_chain=None, target_chain=None, 
                        show_hulls=False, hull_folder=None, output_file=None, output_dir=None):
    """
    Visualize PDB structure in PyMOL
    
    Args:
        pdb_file: Path to PDB file
        design_chain: Chain ID for design chain (e.g., 'G')
        target_chain: Chain ID for target chain (e.g., 'A')
        show_hulls: Whether to overlay convex hulls
        hull_folder: Directory containing hull PDB files
        output_file: Optional PNG output file (relative to output_dir)
        output_dir: Directory for output files (default: pipeline_analysis/scope/pymol_visualizations/)
    """
    # Set output directory
    if output_dir is None:
        repo_root = Path(__file__).parent.parent
        output_dir = repo_root / 'pipeline_analysis' / 'scope' / 'pymol_visualizations'
    else:
        output_dir = Path(output_dir)
    
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Resolve output file path
    if output_file:
        output_file = output_dir / Path(output_file).name
    
    # Initialize PyMOL
    pymol.finish_launching(['pymol', '-cq'])  # -c: no GUI, -q: quiet
    
    # Load structure
    pdb_path = Path(pdb_file)
    if not pdb_path.exists():
        print(f"ERROR: PDB file not found: {pdb_file}")
        return
    
    structure_name = pdb_path.stem
    cmd.load(str(pdb_path), structure_name)
    
    # Color chains
    if design_chain:
        cmd.color('blue', f'{structure_name} and chain {design_chain}')
        cmd.show('cartoon', f'{structure_name} and chain {design_chain}')
    
    if target_chain:
        cmd.color('red', f'{structure_name} and chain {target_chain}')
        cmd.show('cartoon', f'{structure_name} and chain {target_chain}')
    
    # Show sidechains for design chain
    if design_chain:
        cmd.show('sticks', f'{structure_name} and chain {design_chain} and not backbone')
    
    # Load hulls if requested
    if show_hulls and hull_folder:
        hull_folder = Path(hull_folder)
        if hull_folder.exists():
            load_hulls_pymol(hull_folder, structure_name, design_chain, target_chain)
    
    # Set view
    cmd.orient(structure_name)
    cmd.zoom(structure_name)
    
    # Save image if requested
    if output_file:
        cmd.png(str(output_file), width=1920, height=1080, dpi=300, ray=1)
        print(f"Saved image to: {output_file}")
    else:
        # Interactive mode
        print("PyMOL window opened. Close to exit.")
        cmd.quit()


def load_hulls_pymol(hull_folder, structure_name, design_chain=None, target_chain=None):
    """Load convex hull PDB files into PyMOL as transparent surfaces"""
    import glob
    
    hull_files = sorted(glob.glob(str(hull_folder / "Chain*Res*.pdb")))
    
    if not hull_files:
        print(f"No hull files found in {hull_folder}")
        return
    
    print(f"Loading {len(hull_files)} hull files...")
    
    # Load design chain hulls
    if design_chain:
        design_hulls = [f for f in hull_files if f'Chain{design_chain}Res' in f]
        for i, hull_file in enumerate(design_hulls[:10]):  # Limit to first 10 for performance
            hull_name = f"hull_design_{i}"
            cmd.load(str(hull_file), hull_name)
            cmd.color('cyan', hull_name)
            cmd.show('surface', hull_name)
            cmd.set('transparency', 0.7, hull_name)
    
    # Load target chain hulls
    if target_chain:
        target_hulls = [f for f in hull_files if f'Chain{target_chain}Res' in f]
        for i, hull_file in enumerate(target_hulls[:10]):  # Limit to first 10 for performance
            hull_name = f"hull_target_{i}"
            cmd.load(str(hull_file), hull_name)
            cmd.color('yellow', hull_name)
            cmd.show('surface', hull_name)
            cmd.set('transparency', 0.7, hull_name)


def visualize_sequence_alignment(sequences_file=None):
    """Visualize sequence data (if sequence file provided)"""
    if not sequences_file:
        return
    
    # This would require sequence data format
    # Placeholder for sequence visualization
    print("Sequence visualization not yet implemented")


def main():
    parser = argparse.ArgumentParser(description='Visualize OSPREY structures with PyMOL')
    parser.add_argument('--pdb', required=True, help='PDB file to visualize')
    parser.add_argument('--design-chain', default='G', help='Design chain ID')
    parser.add_argument('--target-chain', default='A', help='Target chain ID')
    parser.add_argument('--show-hulls', action='store_true', help='Show convex hulls')
    parser.add_argument('--hull-folder', help='Directory containing hull PDB files')
    parser.add_argument('--output', help='Output PNG filename (saved to output directory)')
    parser.add_argument('--output-dir', help='Output directory (default: pipeline_analysis/scope/pymol_visualizations/)')
    parser.add_argument('--interactive', action='store_true', default=True,
                       help='Open interactive PyMOL window (default)')
    
    args = parser.parse_args()
    
    if args.show_hulls and not args.hull_folder:
        # Try to find hull folder based on PDB name
        pdb_stem = Path(args.pdb).stem
        potential_hulls = [
            f'pdb_hulls_{pdb_stem}',
            f'pdb_hulls_2rl0',  # Default
            'pdb_hulls_scaling_comparison/full_22aa'
        ]
        for hull_path in potential_hulls:
            if Path(hull_path).exists():
                args.hull_folder = hull_path
                print(f"Using hull folder: {hull_path}")
                break
        
        if not args.hull_folder:
            print("WARNING: --show-hulls specified but no hull folder found")
            args.show_hulls = False
    
    if args.output:
        args.interactive = False
    
    visualize_structure(
        args.pdb,
        design_chain=args.design_chain,
        target_chain=args.target_chain,
        show_hulls=args.show_hulls,
        hull_folder=args.hull_folder,
        output_file=args.output,
        output_dir=args.output_dir
    )


if __name__ == '__main__':
    main()

