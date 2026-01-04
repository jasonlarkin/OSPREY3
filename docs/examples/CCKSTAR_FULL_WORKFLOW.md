# CCKStar Full Workflow

## Overview

CCKStar (Convex Closure K*) is a suite of three algorithms for de novo design of L and D peptide binders:

1. **SCOPE**: Geometric analysis using convex hulls to identify residue contacts
2. **MONTAGE**: Scaffold generation using MASTER database and K* evaluation
3. **ARISE**: Iterative sequence design using SCOPE-guided K* searches

## Workflow Architecture

```
Input PDB (2 chains: target + design)
    ↓
[SCOPE] → Geometric analysis (convex hulls)
    ↓
    ├─→ Intra-chain doublets (design chain contacts)
    └─→ Inter-chain contacts (design ↔ target)
    ↓
[MONTAGE] → Scaffold generation
    ↓
    ├─→ MASTER database search
    ├─→ Polyalanine scaffolds
    └─→ K* evaluation → GMEC PDBs
    ↓
[ARISE] → Iterative sequence design
    ↓
    ├─→ Use SCOPE graph (doublets)
    ├─→ Mutate 2 connected residues (400 sequences)
    ├─→ K* evaluation → Best GMEC
    ├─→ Update SCOPE graph
    └─→ Repeat until full sequence
    ↓
Final designed sequences (ranked by binding affinity)
```

## Step 1: SCOPE (Side Chain Orientation and Position Evaluation)

### Purpose
Determine side chain contacts by configuration space (C-space) intersections based on rotameric geometry. This prioritizes mutant and conformation modeling.

### Workflow
1. Generate convex hulls for design chain based on all rotamers for specified amino acids
2. Generate convex hulls for target chain (wildtype identity rotamers)
3. Compute intra-chain intersections (design chain residue pairs)
4. Compute inter-chain intersections (design residues ↔ target residues)
5. Return residue C-space intersections

### Output
- **Doublets**: Intra-chain intersections, returned as pairs (e.g., `{1, 2}`, `{2, 4}`)
- **Islands**: Design chain residues with no intra-chain intersections (may have inter-chain)
- **Inter-chain contacts**: List of target residues intersecting with each design residue
- **Hull PDB files**: Individual and aggregate convex hull files

### Usage
```python
from Find_Doublets import SCOPE, rank_flex_overlap, rank_design_overlap

intrachain_pairs, interchain_pairs = SCOPE(
    pdb_file,           # Input PDB with 2 chains
    output_folder,      # Where to save hull PDBs
    design_chain_id,    # e.g., 'G'
    design_aa_types,    # List of amino acids to consider
    save_pdb=True,      # Save hull files
    design_chirality='L', # 'L' or 'D'
    fixed_residues=[]    # Residues to keep as wildtype
)

# Optional: Rank by volume overlap
flex_order = rank_flex_overlap(design_id, target_id, intrachain_pairs, 
                                interchain_pairs, output_folder)
design_order = rank_design_overlap(design_id, intrachain_pairs, output_folder)
```

### Prerequisites
- Protonated PDB file with exactly 2 chains (target and design)
- Python PyVista library

### See Also
- Detailed SCOPE documentation: `docs/examples/CCKSTAR_SCOPE_ANALYSIS.md`
- Test example: `src/main/python/CCKStar/test_scope_2rl0.py`

## Step 2: MONTAGE (Motif-Oriented Noncanonical Template Assembly and Generation Engine)

### Purpose
Generate L or D polyalanine scaffolds for L targets using MASTER database search and K* evaluation.

### Workflow
1. **MASTER Search**: Perform reflection operations and run MASTER database search
2. **Scaffold Generation**: 
   - Mutate all design residues to ALA (except GLY and PRO)
   - Place VAL hull (approximates ALA + VdW radius) on each design residue
   - Place wildtype hull on each L-target residue
3. **Flexibility Assignment**:
   - Inter-chain hull intersection → flexibility on participating target residue
   - If inter-chain intersects > max_flex, select by hull volume overlap
4. **K* Evaluation**: Run K* algorithm with:
   - Flexible target residues (from inter-chain overlaps)
   - Translation + rotation of design chain
5. **Ranking**: Scaffolds ranked by designability (K* binding affinity)

### Output
- **Scaffold PDBs**: Polyalanine scaffolds from MASTER matches
- **K* Prep Files**: OSPREY configuration files for each scaffold
- **GMEC PDBs**: Global Minimum Energy Conformations after K* optimization
- **MONTAGE_GMEC directory**: Final scaffold structures ready for ARISE

### Usage
```python
from MONTAGE import run_MONTAGE

run_MONTAGE(
    input_pdb_directory,  # Directory with 2-chain PDBs
    input_chirality,      # 'L' or 'D' (design chain chirality)
    output_chirality,     # 'L' or 'D' (desired output)
    master_matches,       # Number of MASTER matches to use
    max_flex              # Max flexible target residues
)
```

### Prerequisites
- MASTER database (see https://grigoryanlab.org/master/)
- Filepaths in `resources/db.txt` pointing to MASTER database PDS files
- Input PDBs: 2 chains only, target listed first (target ID < design ID)
- Targets must be L-space; design chains can be L or D
- OSPREY installed (for K* searches)
- Python PyVista library

### Key Files
- `src/main/python/CCKStar/MONTAGE.py`: Main MONTAGE implementation
- `resources/db.txt`: MASTER database paths
- `resources/K_bash.sh`: K* execution script

## Step 3: ARISE (Affinity-driven Rational Iteration for Sequence Engineering)

### Purpose
Generate high-affinity L or D peptides for L-targets through iterative sequence design.

### Workflow
1. **Graph Construction**: Build undirected graph of potential chemical contacts using SCOPE doublets
2. **Iterative Design** (per round):
   - Select 2 connected design chain residues (from SCOPE graph)
   - Generate 400 sequence variants (20 AA × 20 AA)
   - Set flexibility on all neighboring target residues (from SCOPE inter-chain contacts)
   - Run K* evaluation for each sequence
   - Get GMEC PDB of highest-affinity pair
3. **Graph Update**: Recompute SCOPE graph with new GMEC structure
4. **Repeat**: Continue until full design chain sequence is assigned
5. **Completion**: When all residues are assigned, save final designs

### Output
- **Round directories**: `round_1/`, `round_2/`, etc. with K* results
- **Final designs**: Fully assigned sequences in `final_designs_outfolder/`
- **Design PDBs**: Complete sequences in complex with target

### Usage
```python
from ARISE import maintain_gly_pro, run_ARISE

# Maintain GLY and PRO residues from MONTAGE GMECs
gly_pro_doublets = maintain_gly_pro(design_chain_id)

# Run ARISE iterative design
run_ARISE(
    round_number=1,              # Starting round (1 = start from MONTAGE_GMEC)
    length_chain=12,             # Design chain length
    finished_matches=set(),     # Fully assigned matches (empty to start)
    visited_doublets=gly_pro_doublets,  # Assigned doublets (GLY/PRO from MONTAGE)
    design_id='B',              # Design chain ID
    target_id='A',               # Target chain ID
    final_designs_outfolder='completed-matches',  # Output directory
    apo_tolerance=0.2,           # Apo stability threshold
    design_chirality='L'         # 'L' or 'D'
)
```

### Prerequisites
- `MONTAGE_GMEC/` directory with two-chain PDBs from MONTAGE
- OSPREY installed (for K* searches)
- Python PyVista library
- Cluster access (for parallel K* runs)

### Key Parameters
- **apo_tolerance**: Delta of apo wildtype partition function to apo mutant. Set close to 0 to prevent improving predicted binding (K* ratio) driven by destabilizing the apo ligand.
- **visited_doublets**: Dictionary tracking assigned residue pairs. Start with GLY/PRO from MONTAGE.
- **finished_matches**: Set of matches fully assigned. Grows as ARISE progresses.

### Key Files
- `src/main/python/CCKStar/ARISE.py`: Main ARISE implementation
- `MONTAGE_GMEC/`: Input directory (created by MONTAGE)
- `resources/K_bash.sh`: K* execution script

## Complete Example Workflow

```python
from Find_Doublets import SCOPE, rank_flex_overlap, rank_design_overlap
from MONTAGE import run_MONTAGE
from ARISE import maintain_gly_pro, run_ARISE

# Step 1: SCOPE - Geometric analysis
intrachain_pairs, interchain_pairs = SCOPE(
    '6ov7.pdb', 'pdb_hulls', 'C',
    ['VAL', 'CYS', 'LEU', 'ILE', 'MET', 'TRP', 'PHE', 'LYS', 'ARG', 
     'HID', 'HIE', 'HIP', 'SER', 'THR', 'TYR', 'ASN', 'GLN', 'ASP', 
     'GLU', 'ALA', 'GLY', 'PRO'],
    True, 'L', []
)

# Optional: Rank by volume overlap
flex_order = rank_flex_overlap('C', 'A', intrachain_pairs, 
                                interchain_pairs, 'pdb_hulls')
design_order = rank_design_overlap('C', intrachain_pairs, 'pdb_hulls')

# Step 2: MONTAGE - Scaffold generation
run_MONTAGE("L_to_D", "L", "D", 20, 4)

# Step 3: ARISE - Iterative sequence design
gly_pro_doublets = maintain_gly_pro('B')
run_ARISE(1, 12, set(), gly_pro_doublets, 'B', 'A',
          'completed-matches', 0.2, 'L')
```

## Integration with OSPREY K* Algorithm

CCKStar uses OSPREY's Java K* algorithm for binding affinity prediction:

1. **SCOPE** → Identifies which residues to consider (pruning)
2. **MONTAGE** → Generates scaffolds, evaluates with K*
3. **ARISE** → Iteratively designs sequences, evaluates with K*

K* calculates partition functions for:
- **Bound state**: Design chain + target complex
- **Apo state**: Design chain alone
- **Binding affinity**: Ratio of partition functions (K* = Z_bound / Z_apo)

See `docs/performance/KSTAR_PERFORMANCE_ANALYSIS.md` for K* algorithm details.

## File Structure

```
src/main/python/CCKStar/
├── main.py              # Example workflow
├── Find_Doublets.py     # SCOPE implementation
├── MONTAGE.py           # MONTAGE implementation
├── ARISE.py             # ARISE implementation
├── Make_Convex_Hull.py  # Convex hull generation
├── KStarPrep.py         # OSPREY file preparation
├── resources/
│   ├── db.txt           # MASTER database paths
│   ├── K_bash.sh        # K* execution script
│   └── L-hulls/         # Pre-computed L-space hulls
└── test_scope_2rl0.py   # SCOPE test example
```

## References

- CCKStar README: `src/main/python/CCKStar/README.md`
- SCOPE Analysis: `docs/examples/CCKSTAR_SCOPE_ANALYSIS.md`
- K* Performance: `docs/performance/KSTAR_PERFORMANCE_ANALYSIS.md`
- OSPREY Pipeline: `docs/examples/OSPREY_PIPELINE_EXAMPLES.md`
- MASTER Database: https://grigoryanlab.org/master/

