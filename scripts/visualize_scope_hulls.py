#!/usr/bin/env python3
"""
Visualize SCOPE convex hulls using PyVista

Usage:
    python3 visualize_scope_hulls.py --hull-folder pdb_hulls_2rl0
    python3 visualize_scope_hulls.py --hull-folder pdb_hulls_2rl0 --chain G --residue 638
    python3 visualize_scope_hulls.py --hull-folder pdb_hulls_2rl0 --show-intersections
"""

import argparse
import sys
from pathlib import Path
import numpy as np
import pyvista as pv
from scipy.spatial import ConvexHull
import re
import glob

# Add CCKStar to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src/main/python/CCKStar"))
from Find_Doublets import polydata_from_hull, pdb_to_coords


def load_hull_from_pdb(pdb_file):
    """Load convex hull points from PDB file"""
    coords = pdb_to_coords(str(pdb_file))
    if len(coords) < 4:
        return None  # Need at least 4 points for a 3D convex hull
    return np.array(coords)


def parse_hull_filename(filename):
    """Extract chain and residue ID from hull filename"""
    # Pattern: ChainGRes638.pdb
    match = re.search(r'Chain([A-Z])Res(\d+)\.pdb', Path(filename).name)
    if match:
        return match.group(1), int(match.group(2))
    return None, None


def visualize_hulls(hull_folder, chain=None, residue=None, show_intersections=False, 
                   show_all=False, opacity=0.5):
    """
    Visualize convex hulls from SCOPE output
    
    Args:
        hull_folder: Directory containing hull PDB files
        chain: Specific chain to show (e.g., 'G' or 'A')
        residue: Specific residue to show (e.g., 638)
        show_intersections: Show intersecting hull pairs
        show_all: Show all hulls (may be slow for many hulls)
        opacity: Opacity of hull meshes (0-1)
    """
    hull_folder = Path(hull_folder)
    if not hull_folder.exists():
        print(f"ERROR: Hull folder not found: {hull_folder}")
        return
    
    # Find all hull PDB files
    hull_files = sorted(glob.glob(str(hull_folder / "Chain*Res*.pdb")))
    
    if not hull_files:
        print(f"ERROR: No hull files found in {hull_folder}")
        return
    
    print(f"Found {len(hull_files)} hull files")
    
    # Create PyVista plotter
    plotter = pv.Plotter()
    plotter.set_background('white')
    
    # Color scheme
    colors = {
        'G': 'blue',    # Design chain
        'A': 'red',     # Target chain
        'default': 'gray'
    }
    
    hulls_data = {}
    
    # Load hulls
    print("Loading hulls...")
    for hull_file in hull_files:
        chain_id, resid = parse_hull_filename(hull_file)
        if chain_id is None:
            continue
        
        # Filter by chain/residue if specified
        if chain and chain_id != chain:
            continue
        if residue and resid != residue:
            continue
        
        coords = load_hull_from_pdb(hull_file)
        if coords is None or len(coords) < 4:
            continue
        
        if chain_id not in hulls_data:
            hulls_data[chain_id] = {}
        
        hulls_data[chain_id][resid] = {
            'coords': coords,
            'file': hull_file
        }
    
    print(f"Loaded {sum(len(h) for h in hulls_data.values())} hulls")
    
    # Visualize hulls
    if show_all or (chain and residue):
        # Show all loaded hulls
        for chain_id, residues in hulls_data.items():
            color = colors.get(chain_id, colors['default'])
            for resid, data in residues.items():
                try:
                    mesh = polydata_from_hull(data['coords'])
                    plotter.add_mesh(mesh, color=color, opacity=opacity, 
                                   label=f"Chain {chain_id} Res {resid}")
                    print(f"  Added: Chain {chain_id} Res {resid} ({len(data['coords'])} points)")
                except Exception as e:
                    print(f"  Warning: Failed to create mesh for Chain {chain_id} Res {resid}: {e}")
    elif chain:
        # Show all residues for specified chain
        if chain in hulls_data:
            color = colors.get(chain, colors['default'])
            for resid, data in hulls_data[chain].items():
                try:
                    mesh = polydata_from_hull(data['coords'])
                    plotter.add_mesh(mesh, color=color, opacity=opacity,
                                   label=f"Chain {chain} Res {resid}")
                except Exception as e:
                    print(f"  Warning: Failed to create mesh for Chain {chain} Res {resid}: {e}")
    else:
        # Show representative hulls (one per chain)
        for chain_id, residues in hulls_data.items():
            if not residues:
                continue
            # Show first residue as representative
            resid = sorted(residues.keys())[0]
            data = residues[resid]
            color = colors.get(chain_id, colors['default'])
            try:
                mesh = polydata_from_hull(data['coords'])
                plotter.add_mesh(mesh, color=color, opacity=opacity,
                               label=f"Chain {chain_id} (showing Res {resid} as example)")
            except Exception as e:
                print(f"  Warning: Failed to create mesh for Chain {chain_id} Res {resid}: {e}")
    
    # Show intersections if requested
    if show_intersections and len(hulls_data) >= 2:
        print("\nComputing intersections...")
        chains = list(hulls_data.keys())
        if len(chains) >= 2:
            design_chain = chains[0]
            target_chain = chains[1]
            
            # Find intersecting pairs (simplified - check all pairs)
            intersection_count = 0
            for des_resid, des_data in hulls_data[design_chain].items():
                for tar_resid, tar_data in hulls_data[target_chain].items():
                    try:
                        mesh1 = polydata_from_hull(des_data['coords'])
                        mesh2 = polydata_from_hull(tar_data['coords'])
                        intersection = mesh1.boolean_intersection(mesh2)
                        
                        if intersection.n_cells > 0:
                            # Show intersection in yellow
                            plotter.add_mesh(intersection, color='yellow', opacity=0.8,
                                           label=f"Intersection: {design_chain}{des_resid} × {target_chain}{tar_resid}")
                            intersection_count += 1
                            if intersection_count >= 10:  # Limit to avoid clutter
                                print("  (Limiting to first 10 intersections)")
                                break
                    except Exception as e:
                        continue
                if intersection_count >= 10:
                    break
            
            print(f"  Found {intersection_count} intersections")
    
    # Add legend
    plotter.add_legend(labels=[f"Chain {c}" for c in hulls_data.keys()])
    
    # Set camera and show
    plotter.camera_position = 'iso'
    plotter.show()
    
    print("\nVisualization complete!")


def main():
    parser = argparse.ArgumentParser(description='Visualize SCOPE convex hulls')
    parser.add_argument('--hull-folder', required=True,
                       help='Directory containing hull PDB files (e.g., pdb_hulls_2rl0)')
    parser.add_argument('--chain', choices=['G', 'A'],
                       help='Show specific chain only')
    parser.add_argument('--residue', type=int,
                       help='Show specific residue only (requires --chain)')
    parser.add_argument('--show-intersections', action='store_true',
                       help='Show intersecting hull pairs')
    parser.add_argument('--show-all', action='store_true',
                       help='Show all hulls (may be slow)')
    parser.add_argument('--opacity', type=float, default=0.5,
                       help='Opacity of hull meshes (0-1, default: 0.5)')
    
    args = parser.parse_args()
    
    if args.residue and not args.chain:
        print("ERROR: --residue requires --chain")
        sys.exit(1)
    
    visualize_hulls(
        args.hull_folder,
        chain=args.chain,
        residue=args.residue,
        show_intersections=args.show_intersections,
        show_all=args.show_all,
        opacity=args.opacity
    )


if __name__ == '__main__':
    main()

