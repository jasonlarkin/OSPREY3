# PyMOL Visualization Guide for OSPREY

## Installation

PyMOL installation location is system-specific. Set one of:
- `PYMOL_CMD` (preferred): full path to the `pymol` executable
- `PYMOL_PYTHON` (optional): a Python interpreter that has PyMOL available

## Basic Usage

### Command Line (Non-Interactive)

```bash
# Visualize structure
python3 scripts/visualize_with_pymol.py --pdb examples/python.KStar/2RL0.min.reduce.pdb --output structure.png

# Visualize with hulls
python3 scripts/visualize_with_pymol.py \
    --pdb examples/python.KStar/2RL0.min.reduce.pdb \
    --show-hulls \
    --hull-folder pdb_hulls_2rl0 \
    --output structure_with_hulls.png
```

### Interactive PyMOL

```bash
# Open interactive window
python3 scripts/visualize_with_pymol.py \
    --pdb examples/python.KStar/2RL0.min.reduce.pdb \
    --design-chain G \
    --target-chain A \
    --show-hulls \
    --hull-folder pdb_hulls_2rl0
```

## PyMOL Python API Examples

### Load and Color Structure

```python
import pymol
from pymol import cmd

pymol.finish_launching(['pymol', '-cq'])
cmd.load('examples/python.KStar/2RL0.min.reduce.pdb', 'structure')
cmd.color('blue', 'structure and chain G')  # Design chain
cmd.color('red', 'structure and chain A')     # Target chain
cmd.show('cartoon', 'structure')
cmd.show('sticks', 'structure and chain G and not backbone')
cmd.orient('structure')
```

### Visualize Convex Hulls

```python
# Load hull PDB files
cmd.load('pdb_hulls_2rl0/ChainGRes638.pdb', 'hull_g638')
cmd.show('surface', 'hull_g638')
cmd.set('transparency', 0.7, 'hull_g638')
cmd.color('cyan', 'hull_g638')
```

### Save Images

```python
# High-resolution PNG
cmd.png('output.png', width=1920, height=1080, dpi=300, ray=1)

# Ray-traced (slower, higher quality)
cmd.ray(1920, 1080)
cmd.png('output_ray.png')
```

## Common PyMOL Commands for OSPREY

### Structure Display

```python
# Show cartoon representation
cmd.show('cartoon', 'all')

# Show sticks for sidechains
cmd.show('sticks', 'chain G and not backbone')

# Show spheres for atoms
cmd.show('spheres', 'chain G and name CA')

# Hide everything
cmd.hide('all')
```

### Coloring

```python
# Color by chain
cmd.color('blue', 'chain G')
cmd.color('red', 'chain A')

# Color by element
cmd.color('yellow', 'elem C')
cmd.color('red', 'elem O')
cmd.color('blue', 'elem N')

# Color by B-factor (if available)
cmd.spectrum('b', 'blue_white_red', 'all')
```

### Selection

```python
# Select design chain residues
cmd.select('design', 'chain G and resi 638-654')

# Select interface residues (within 5A of target)
cmd.select('interface', 'chain G within 5 of chain A')

# Select specific residue
cmd.select('res638', 'chain G and resi 638')
```

### Measurements

```python
# Measure distance
cmd.distance('dist1', 'chain G and resi 638 and name CA', 
             'chain A and resi 241 and name CA')

# Measure angle
cmd.angle('angle1', 'chain G and resi 638', 
          'chain G and resi 639', 
          'chain G and resi 640')
```

## Visualizing SCOPE Results

### Show Design Chain with Flexibility

```python
cmd.load('examples/python.KStar/2RL0.min.reduce.pdb', 'structure')
cmd.color('blue', 'chain G')
cmd.show('cartoon', 'chain G')
cmd.show('sticks', 'chain G and not backbone')

# Load representative hulls
for res in [638, 640, 642, 644]:
    cmd.load(f'pdb_hulls_2rl0/ChainGRes{res}.pdb', f'hull_{res}')
    cmd.show('surface', f'hull_{res}')
    cmd.set('transparency', 0.6, f'hull_{res}')
    cmd.color('cyan', f'hull_{res}')
```

### Highlight Intersecting Residues

```python
# Based on SCOPE results, highlight residues that intersect
intersecting_residues = [638, 640, 642, 644, 646, 647, 650, 652, 653]

for res in intersecting_residues:
    cmd.select(f'res{res}', f'chain G and resi {res}')
    cmd.color('yellow', f'res{res}')
    cmd.show('sticks', f'res{res}')
```

## Integration with OSPREY Pipeline

### After SCOPE

```python
# Visualize SCOPE results
pdb_file = 'examples/python.KStar/2RL0.min.reduce.pdb'
hull_folder = 'pdb_hulls_2rl0'

cmd.load(pdb_file, 'structure')
cmd.color('blue', 'chain G')
cmd.color('red', 'chain A')

# Load intersecting hulls (from SCOPE output)
# G638 intersects with A241
cmd.load(f'{hull_folder}/ChainGRes638.pdb', 'hull_g638')
cmd.load(f'{hull_folder}/ChainARes241.pdb', 'hull_a241')
cmd.show('surface', 'hull_g638 or hull_a241')
cmd.set('transparency', 0.5)
```

### After K* Results

```python
# Visualize top sequences from K* results
# Load structure with mutations
cmd.load('mutated_structure.pdb', 'mutant')
cmd.color('green', 'mutant and chain G')
cmd.align('mutant', 'structure')
```

## Performance Tips

1. **Limit hull loading**: Only load representative hulls (every Nth residue)
2. **Use surfaces sparingly**: Surfaces are expensive, use for key residues
3. **Ray-tracing**: Only for final images, not during exploration
4. **Batch operations**: Load multiple structures in one session

## Output Formats

```python
# PNG (raster)
cmd.png('output.png', width=1920, height=1080, dpi=300)

# PNG with ray-tracing
cmd.ray(1920, 1080)
cmd.png('output_ray.png')

# PSE (PyMOL session - saves state)
cmd.save('session.pse')

# PDB (export structure)
cmd.save('exported.pdb', 'structure')
```

## Troubleshooting

### PyMOL not found
- Check installation path (varies by platform and install method)
- Add to PATH if needed (not recommended per installer)
- Use full path to PyMOL executable

### Import errors
- Ensure PyMOL Python module is in Python path
- May need to use PyMOL's bundled Python (install-dependent)

### Hull files not loading
- Check PDB format (PyMOL expects standard PDB format)
- Hull files may need conversion from OSPREY format
- Try loading individual hull files first

