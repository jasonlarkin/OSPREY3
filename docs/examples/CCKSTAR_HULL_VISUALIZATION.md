# SCOPE Convex Hull Visualization

## Overview

This document describes how to visualize SCOPE convex hulls for understanding geometric contacts and debugging the SCOPE algorithm.

## Visualization Methods

### 1. PyVista (Interactive 3D)

**Script**: `src/main/python/CCKStar/visualize_hulls.py`

PyVista provides interactive 3D visualization with rotation, zoom, and transparency controls.

#### Installation
```bash
pip install pyvista scipy numpy
```

#### Usage Examples

**Visualize a doublet (two design residues)**:
```bash
python visualize_hulls.py pdb_hulls_2rl0/ \
    --mode doublet \
    --chain G \
    --res1 638 \
    --res2 639 \
    --target-chain A \
    --target-res 223 240 241
```

**Visualize all hulls for a chain** (limited to avoid clutter):
```bash
python visualize_hulls.py pdb_hulls_2rl0/ \
    --mode all \
    --chain G \
    --max-hulls 10
```

**Visualize inter-chain contact**:
```bash
python visualize_hulls.py pdb_hulls_2rl0/ \
    --mode interchain \
    --chain G \
    --res1 638 \
    --target-chain A \
    --target-res 223 240 241
```

#### Features
- **Color coding**: Different colors for different residues
- **Transparency**: Adjustable opacity to see overlapping hulls
- **Intersection highlighting**: Red mesh shows intersection volume
- **Interactive**: Rotate, zoom, pan with mouse
- **Legend**: Shows which color corresponds to which residue

### 2. PyMOL (Molecular Visualization)

PyMOL is a professional molecular visualization tool that can load PDB files directly.

#### Loading Hull PDBs in PyMOL

```python
# In PyMOL command line or script
load pdb_hulls_2rl0/ChainG_all_hulls.pdb, design_hulls
load pdb_hulls_2rl0/ChainA_all_hulls.pdb, target_hulls

# Color by chain
color blue, design_hulls
color orange, target_hulls

# Show as surface
show surface, design_hulls
show surface, target_hulls

# Adjust transparency
set transparency, 0.5, design_hulls
set transparency, 0.5, target_hulls
```

#### Individual Residue Hulls

```python
# Load specific residue hulls
load pdb_hulls_2rl0/ChainGRes638.pdb, res638
load pdb_hulls_2rl0/ChainGRes639.pdb, res639

# Show as mesh
show mesh, res638
show mesh, res639

# Color differently
color blue, res638
color green, res639
```

#### Creating a Visualization Script

Save as `visualize_hulls_pymol.py`:

```python
#!/usr/bin/env python3
"""PyMOL script to visualize SCOPE hulls."""

import sys
from pymol import cmd

def visualize_doublet(hull_folder, chain_id, res1, res2):
    """Load and visualize a doublet in PyMOL."""
    hull1 = f"{hull_folder}/Chain{chain_id}Res{res1}.pdb"
    hull2 = f"{hull_folder}/Chain{chain_id}Res{res2}.pdb"
    
    cmd.load(hull1, f"res{res1}")
    cmd.load(hull2, f"res{res2}")
    
    cmd.show("mesh", f"res{res1}")
    cmd.show("mesh", f"res{res2}")
    
    cmd.color("blue", f"res{res1}")
    cmd.color("green", f"res{res2}")
    
    cmd.set("transparency", 0.5)

if __name__ == '__main__':
    if len(sys.argv) < 5:
        print("Usage: pymol -c visualize_hulls_pymol.py <hull_folder> <chain_id> <res1> <res2>")
        sys.exit(1)
    
    visualize_doublet(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4])
    cmd.zoom()
```

Run with:
```bash
pymol -c visualize_hulls_pymol.py pdb_hulls_2rl0/ G 638 639
```

### 3. Matplotlib (Static 2D Projections)

For quick 2D projections or publication figures:

```python
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
from Find_Doublets import pdb_to_coords
import numpy as np

def plot_hull_2d_projection(pdb_file, ax, color='blue', alpha=0.3):
    """Plot 2D projection of hull."""
    coords = pdb_to_coords(pdb_file)
    if len(coords) < 4:
        return
    
    coords = np.array(coords)
    
    # Project to XY plane
    ax.scatter(coords[:, 0], coords[:, 1], c=color, alpha=alpha, s=10)
    
    # Draw convex hull
    from scipy.spatial import ConvexHull
    hull = ConvexHull(coords[:, :2])
    for simplex in hull.simplices:
        ax.plot(coords[simplex, 0], coords[simplex, 1], color=color, alpha=0.5)

fig, ax = plt.subplots()
plot_hull_2d_projection('pdb_hulls_2rl0/ChainGRes638.pdb', ax, 'blue')
plot_hull_2d_projection('pdb_hulls_2rl0/ChainGRes639.pdb', ax, 'green')
plt.show()
```

## Understanding the Visualizations

### Hull Shape
- **Size**: Larger hulls indicate more rotameric flexibility
- **Shape**: Elongated hulls suggest preferred side chain orientations
- **Position**: Relative positions show spatial relationships

### Intersections
- **Doublet intersections**: Overlapping design chain hulls indicate contacts
- **Inter-chain intersections**: Overlapping design and target hulls indicate binding interfaces
- **Volume**: Larger intersection volumes suggest stronger contacts

### Color Coding
- **Blue**: Design chain residues
- **Green**: Second design residue in doublet
- **Orange/Red**: Target chain residues
- **Red**: Intersection volumes

## Example: Visualizing 2RL0 Results

Based on the 2RL0 SCOPE analysis:

### Top Doublet (G644 × G646: 152.98 Å³)
```bash
python visualize_hulls.py pdb_hulls_2rl0/ \
    --mode doublet \
    --chain G \
    --res1 644 \
    --res2 646
```

### Top Inter-chain Contact (G650 × A191: 152.89 Å³)
```bash
python visualize_hulls.py pdb_hulls_2rl0/ \
    --mode interchain \
    --chain G \
    --res1 650 \
    --target-chain A \
    --target-res 191
```

## Troubleshooting

### "Hull file not found"
- Check that SCOPE was run with `savePDB=True`
- Verify the hull folder path is correct
- Ensure residue IDs match (not positions)

### "Could not create hull mesh"
- Hull may have too few points (< 4)
- Try loading the aggregate file (`ChainG_all_hulls.pdb`) instead

### Performance Issues
- Limit number of hulls visualized (`--max-hulls`)
- Use aggregate files for overview
- Visualize specific doublets/contacts individually

## Integration with Analysis

Visualization helps with:
1. **Debugging**: Verify SCOPE is finding expected contacts
2. **Understanding**: See why certain doublets are selected
3. **Optimization**: Identify opportunities to reduce flexibility
4. **Documentation**: Create figures for papers/presentations

## References

- PyVista: https://docs.pyvista.org/
- PyMOL: https://pymol.org/
- SCOPE Analysis: `docs/examples/CCKSTAR_SCOPE_ANALYSIS.md`
- Convex Hulls: `docs/examples/CCKSTAR_SCOPE_CONVEX_HULLS.md`

