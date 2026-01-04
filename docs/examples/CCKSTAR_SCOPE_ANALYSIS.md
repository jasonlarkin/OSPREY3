# CCKStar SCOPE Analysis

## Overview

SCOPE (Spatial Convex Hull Optimization for Protein Engineering) is a geometric analysis algorithm in CCKStar that identifies residue-residue contacts using convex hulls. This document describes the SCOPE implementation, usage, and findings from testing with the 2RL0 example.

## What SCOPE Does

SCOPE performs geometric analysis to identify:

1. **Intra-chain contacts (doublets)**: Pairs of residues within the design chain whose convex hulls intersect
2. **Inter-chain contacts**: Design chain residues that intersect with target chain residues
3. **Convex hull generation**: Creates 3D convex hulls for each residue considering all possible amino acid mutations

## Test Example: 2RL0

### Input
- **PDB File**: `examples/python.KStar/2RL0.min.reduce.pdb`
- **Design Chain**: G (protein, residues 638-654)
- **Target Chain**: A (ligand, residues 153-241)
- **Amino Acid Types**: 22 standard amino acids (VAL, CYS, LEU, ILE, MET, TRP, PHE, LYS, ARG, HID, HIE, HIP, SER, THR, TYR, ASN, GLN, ASP, GLU, ALA, GLY, PRO)

### Output

#### Intra-chain Doublets
Found 17 doublets (residue pairs with intersecting convex hulls):
```
{1, 2}, {2, 4}, {3, 5}, {4, 6}, {5, 7}, {8, 6}, {9, 7}, {8, 10}, 
{9, 11}, {10, 12}, {10, 14}, {11, 13}, {12, 14}, {13, 15}, {14, 15}, 
{16, 14}, {17, 15}
```

#### Inter-chain Contacts
Each design chain residue (G638-G654) mapped to intersecting target chain residues (A153-A241):
- G638 intersects with A[223, 240, 241]
- G639 intersects with A[221, 241]
- G640 intersects with A[238, 240]
- ... (see test output for full list)

#### Generated Files
- Individual hull PDB files: `ChainGRes638.pdb`, `ChainGRes639.pdb`, etc.
- Aggregate hull files: `ChainG_all_hulls.pdb`, `ChainA_all_hulls.pdb`

## Residue ID Mapping Issue

### Problem

SCOPE uses **two different numbering systems**:

1. **1-indexed positions** in doublets: `{1, 2}` refers to the first and second residues in the design chain
2. **Actual PDB residue IDs** in file names: `ChainGRes638.pdb` refers to residue 638 in the PDB

### Mapping

For the 2RL0 example:
- **Design chain (G)**: Position 1 → Residue 638, Position 2 → Residue 639, ..., Position 17 → Residue 654
- **Target chain (A)**: Position 1 → Residue 153, Position 2 → Residue 154, ..., Position 89 → Residue 241

### Impact

The ranking functions (`rank_flex_overlap()` and `rank_design_overlap()`) expect 1-indexed positions in file names but files are saved with actual residue IDs. This causes `FileNotFoundError` when trying to rank overlaps.

### Solution

**FIXED**: The ranking functions (`rank_flex_overlap()` and `rank_design_overlap()`) have been updated to automatically extract residue ID mappings from hull file names in the output directory. They now:

1. Scan the hull folder for files matching the pattern `Chain{ID}Res{number}.pdb`
2. Extract residue IDs from filenames and map them to 1-indexed positions
3. Use the mapping to locate the correct hull files for volume overlap calculations

The functions now work correctly with actual residue IDs in file names while accepting 1-indexed positions in the doublet/interchain data structures.

## Code Location

- **SCOPE function**: `src/main/python/CCKStar/Find_Doublets.py`
- **Test script**: `src/main/python/CCKStar/test_scope_2rl0.py`
- **Main CCKStar workflow**: `src/main/python/CCKStar/main.py`

## Usage

```python
from Find_Doublets import SCOPE

intrachain_pairs, interchain_pairs = SCOPE(
    pdb_file,
    output_folder,
    design_chain_id,      # e.g., 'G'
    design_aa_types,      # List of amino acid types
    save_pdb=True,        # Save hull PDB files
    design_chirality='L', # 'L' or 'D'
    fixed_residues=[]     # List of fixed residue IDs
)
```

## Integration with CCKStar Workflow

SCOPE is the first step in the CCKStar workflow:

1. **SCOPE**: Geometric analysis to identify contacts (this document)
2. **Iterative Design**: Uses SCOPE output to guide sequence design
3. **K* Calculation**: Java K* algorithm evaluates binding affinity for candidate sequences

The doublets and inter-chain contacts from SCOPE are used to:
- Prune the search space (only consider residues with contacts)
- Prioritize flexible residues by volume overlap
- Guide the iterative design process

## References

- CCKStar main workflow: `src/main/python/CCKStar/main.py`
- K* algorithm: `src/main/java/edu/duke/cs/osprey/kstar/`
- OSPREY pipeline examples: `docs/examples/OSPREY_PIPELINE_EXAMPLES.md`

