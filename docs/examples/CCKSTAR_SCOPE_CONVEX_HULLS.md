# SCOPE Convex Hull Generation

## Overview

SCOPE uses convex hulls to represent the 3D space that amino acid side chains can occupy across all possible rotamers. This geometric approach allows efficient identification of residue-residue contacts without enumerating all possible conformations.

## Convex Hull Generation Process

### 1. Rotamer Library

SCOPE uses the **Lovell Rotamer Library** (also known as "The Penultimate Rotamer Library"), which contains:
- Pre-computed rotamer conformations for all standard amino acids
- Multiple rotamers per amino acid (e.g., VAL has 3: p, t, m; LYS has 25+)
- L-space and D-space variants

**Location**: `src/main/python/CCKStar/resources/RotamerConstructs.py`

**Example rotamers**:
- VAL: 3 rotamers (p, t, m)
- LEU: 5 rotamers (pp, tp, tt, mp, mt)
- LYS: 25 rotamers (ptpt, pttp, pttt, ...)
- ARG: 30+ rotamers

### 2. Hull Construction Algorithm

The `make_convex_hull()` function (`Make_Convex_Hull.py`) performs:

```python
def make_convex_hull(AA_types: list, outname, savePDB, chirality):
    # 1. Collect all rotamers for specified amino acids
    all_rotamers = []
    for aa in AA_types:
        if aa not in ['GLY', 'ALA']:  # Skip (too small for meaningful hulls)
            rotamers = get_rotamers(aa)
            all_rotamers.append(rotamers)
    
    # 2. Reflect to D-space if needed
    if chirality == 'D':
        for rotamer_set in all_rotamers:
            for rot in rotamer_set:
                reflect_rotamer(rot)
    
    # 3. Translate rotamers to origin (CA at [0,0,0])
    for rotamer_set in all_rotamers:
        for rot in rotamer_set:
            translate_rotamer(rot, [0, 0, 0])
    
    # 4. Rotate to standard alignment (VALp reference)
    target_rotation = VALp_L_origin if chirality == 'L' else VALp_D_origin
    for rotamer_set in all_rotamers:
        for rot in rotamer_set:
            rotate_rotamer(rot, target_rotation)
    
    # 5. Collect all side chain atom coordinates
    coords_holder = []
    for rotamer_set in all_rotamers:
        for rot in rotamer_set:
            # Extract all side chain atom coordinates
            for atom_name in rot.get_sidechain_atoms():
                coords_holder.append(getattr(rot, atom_name))
    
    # 6. Compute convex hull using scipy
    hull = ScipyConvexHull(coords_holder)
    hull_points = hull.vertices  # Extract hull vertices
    
    return hull_points
```

### 3. Key Steps Explained

#### Alignment to Origin
- All rotamers are aligned so that:
  - **CA (C-alpha)** is at origin `[0, 0, 0]`
  - **N, C, HA** backbone atoms are in standard positions
  - This allows consistent comparison across different residues

#### Standard Reference Frame
- Uses **VALp rotamer** as the reference alignment
- VALp is the most common rotamer for valine
- Provides a consistent coordinate system for L-space and D-space

#### Chirality Handling
- **L-space**: Standard left-handed amino acids
- **D-space**: Right-handed (mirrored) amino acids
- Reflection operation converts L rotamers to D rotamers

#### Convex Hull Computation
- Uses **scipy.spatial.ConvexHull** (Qhull algorithm)
- Computes the smallest convex polyhedron containing all rotamer atoms
- Returns vertices of the hull (extreme points)

### 4. Hull Placement on Residues

In `Find_Doublets.py`, the `insert_hulls()` function:

1. **Reads backbone coordinates** from PDB (N, CA, C, HA)
2. **Creates ConvexHull object** with:
   - Rotamer hull points (from `make_convex_hull()`)
   - Backbone anchor coordinates
3. **Transforms hull** to residue position:
   - Translates to residue's CA position
   - Rotates to match residue's backbone orientation
   - Uses `moveCH()` method to align hull with backbone

```python
class ConvexHull:
    def __init__(self, resid, N_back, CA_back, C_back, HA_back, hull_coords):
        self.resid = resid
        self.N_back = N_back
        self.CA_back = CA_back
        self.C_back = C_back
        self.HA_back = HA_back
        self.hull_coords = hull_coords  # Transformed to residue position
    
    def moveCH(self, N_target, CA_target, C_target, HA_target):
        # Transform hull from origin to actual residue position
        # Uses backbone atoms for alignment
```

## Volume Overlap Calculation

### Two-Pass Algorithm

1. **Quick Check** (fast, approximate):
   - Convert hulls to PyVista meshes
   - Use VTK boolean intersection
   - If intersection exists → proceed to exact calculation
   - If no intersection → return 0.0

2. **Exact Calculation** (slow, precise):
   - Compute intersection polyhedron
   - Calculate volume using tetrahedral decomposition
   - Returns volume in cubic Angstroms (Å³)

### Code Location
- `Find_Doublets.py`: `find_volume_overlap()` and `calculate_volume_overlap()`

## Usage in SCOPE

### Design Chain Hulls
- Generated for **all specified amino acid types**
- Represents union of all possible side chain positions
- Example: If `design_AA_type = ['VAL', 'LEU', 'ILE']`, hull contains all rotamers from all three amino acids

### Target Chain Hulls
- Generated for **wildtype identity only** (`['wt']`)
- Represents space occupied by the actual amino acid at that position
- Used to identify which target residues should be flexible

### Intersection Detection
- **Intra-chain**: Design residue hulls intersecting with each other → doublets
- **Inter-chain**: Design residue hulls intersecting with target residue hulls → flexible target residues

## Example: 2RL0 Analysis

For the 2RL0 example:
- **Design chain G**: 17 residues (638-654)
- **22 amino acid types**: VAL, CYS, LEU, ILE, MET, TRP, PHE, LYS, ARG, HID, HIE, HIP, SER, THR, TYR, ASN, GLN, ASP, GLU, ALA, GLY, PRO
- **Hull generation**: Each design residue gets a hull containing all rotamers from all 22 amino acids
- **Result**: 17 doublets (intra-chain contacts) and inter-chain contacts with target chain A

## Performance Considerations

### Computational Cost
- **Hull generation**: O(n log n) where n = number of rotamer atoms
- **Intersection detection**: O(m) where m = number of hull vertices
- **Volume calculation**: O(k³) where k = intersection complexity

### Optimization Strategies
1. **Pre-computed hulls**: L-space hulls stored in `resources/L-hulls/`
2. **Quick intersection check**: VTK boolean before expensive volume calculation
3. **Parallel processing**: Can be parallelized across residues

## Integration with OSPREY

SCOPE hulls are used to:
1. **Prune search space**: Only consider residues with contacts
2. **Set flexibility**: Target residues with inter-chain intersections become flexible
3. **Guide design**: Doublets indicate which residues should be designed together

## References

- Rotamer Library: `src/main/python/CCKStar/resources/RotamerConstructs.py`
- Hull Generation: `src/main/python/CCKStar/Make_Convex_Hull.py`
- Hull Placement: `src/main/python/CCKStar/Find_Doublets.py` (ConvexHull class)
- Lovell Library: "The Penultimate Rotamer Library" (Osprey 3)

