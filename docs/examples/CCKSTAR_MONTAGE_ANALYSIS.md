# MONTAGE Algorithm Analysis

## Overview

MONTAGE (Motif-Oriented Noncanonical Template Assembly and Generation Engine) generates L or D polyalanine scaffolds for L-targets using the MASTER database and K* evaluation. This document details how MONTAGE works and its integration with SCOPE and K*.

## MONTAGE Workflow

```
Input: Directory of 2-chain PDBs (target + design)
    ↓
[MASTER Search] → Find structural matches in database
    ↓
[Scaffold Generation] → Create polyalanine scaffolds
    ↓
[SCOPE Analysis] → Determine target flexibility
    ↓
[K* Evaluation] → Optimize scaffolds and rank by designability
    ↓
Output: GMEC PDBs ranked by binding affinity
```

## Step-by-Step Process

### 1. MASTER Database Search

**Function**: `run_MASTER()` in `MONTAGE.py:272-305`

MASTER (Motif Alignment and Search Tool) is a structural alignment database that finds similar backbone conformations.

#### Process:
1. **Extract design chain** from input PDB
2. **Create query PDS file** (MASTER format)
3. **Run MASTER search** against database:
   ```bash
   ./resources/master --query query.pds --targetList <database> --topN <matches>
   ```
4. **Get top N matches** (typically 20-100)
5. **Extract matched peptide structures**

#### Chirality Handling:
- **L → L**: Direct use of MASTER matches
- **L → D**: Reflect design chain across Z-axis (mirror)
- **D → L**: Reflect design chain (convert back)
- **D → D**: Reflect, search, reflect back

**Code**:
```python
if input_chirality == 'L' and output_chirality == 'D':
    # Reflect L-peptide to D-space
    reflect_design(complex_pdb, targetID, designID)
elif input_chirality == 'D' and output_chirality == 'L':
    # Reflect D-peptide to L-space
    reflect_design(complex_pdb, targetID, designID)
```

### 2. Scaffold Generation

**Function**: `run_MASTER()` continues

For each MASTER match:
1. **Mutate to polyalanine**: All design residues → ALA (except GLY/PRO)
2. **Align with target**: MASTER already provides alignment
3. **Protonate**: Add hydrogens using OSPREY
4. **Save scaffold PDB**: Ready for K* evaluation

**Key Point**: Scaffolds are **polyalanine** (minimal side chains) to focus on backbone geometry.

### 3. SCOPE Analysis for Flexibility

**Function**: `target_flex_MONTAGE()` in `MONTAGE.py:331-354`

MONTAGE uses SCOPE to determine which target residues should be flexible during K* evaluation:

```python
def target_flex_MONTAGE(pdb_name, design_chirality, max_flex):
    # Run SCOPE with VAL hull (approximates ALA + VdW radius)
    intrachain_contacts, interchain_contacts = SCOPE(
        pdb_name, tmpdir, 'B', ['VAL'], True, design_chirality, []
    )
    
    # Rank by volume overlap
    target_flex = rank_flex_overlap('B', 'A', intrachain_contacts,
                                    interchain_contacts, tmpdir)
    
    # Select top N by overlap
    ordered_interchain = [t for (d, t) in target_flex]
    selected_flex = ordered_interchain[:max_flex]
    
    return selected_flex
```

**Why VAL hull?**
- VAL is similar size to ALA + van der Waals radius
- Provides conservative estimate of space occupied by polyalanine scaffold
- Identifies target residues that could interact with scaffold

**Flexibility Selection**:
- Ranked by volume overlap (largest first)
- Limited to `max_flex` residues (typically 2-4)
- Ensures K* calculations remain tractable

### 4. K* File Preparation

**Function**: `run_MONTAGE()` in `MONTAGE.py:450-523`

For each scaffold:
1. **Get flexible target residues** from SCOPE
2. **Identify GLY/PRO residues** to maintain
3. **Prepare OSPREY files**:
   - Conformation space definitions
   - Force field parameters
   - K* search settings
4. **Set up translation/rotation** of design chain
5. **Compile conformation spaces**

**Code**:
```python
# Find flex on L-target
target_flex = target_flex_MONTAGE(scaff, output_chirality, max_flex)

# Maintain GLY and PRO residues
gly_pro = find_gly_pro_index(scaff, "B")

# Prep K* files
osprey_fileprep_kstar(
    scaff, out_match_foldername, [],
    [ConfSpaceSpecs(doublet=[], flexset=target_flex, mutations=[],
                    graph_path=[])],
    'B', output_chirality, False, True,
    True, True, 'resources/K_bash.sh',
    gly_pro
)
```

### 5. K* Evaluation

**Function**: `cluster_runner_MONTAGE()` in `MONTAGE.py:419-447`

K* is run on each scaffold to:
1. **Optimize conformation**: Find GMEC (Global Minimum Energy Conformation)
2. **Calculate binding affinity**: K* = Z_bound / Z_apo
3. **Rank scaffolds**: By designability (binding affinity)

**Execution**:
- Runs on cluster (SLURM)
- Monitors completion
- Extracts GMEC PDBs for successful runs (K* > 0)

### 6. GMEC Extraction

**Function**: `get_MONTAGE_gmec()` in `MONTAGE.py:357-392`

After K* completes:
1. **Parse log files** for K* scores
2. **Filter successful runs** (K* > 0)
3. **Extract GMEC PDBs** from ensemble outputs
4. **Save to `MONTAGE_GMEC/`** directory
5. **Rank by K* score**

**Output Format**:
```
MONTAGE_GMEC/
  match1-complex.pdb  (K* = 1.23)
  match2-complex.pdb  (K* = 0.87)
  match3-complex.pdb  (K* = 0.45)
  ...
```

## Key Design Decisions

### 1. Polyalanine Scaffolds

**Rationale**:
- Focuses on backbone geometry (from MASTER)
- Minimizes side chain complexity
- Allows K* to optimize backbone placement
- Side chains added later in ARISE

### 2. Limited Flexibility

**Rationale**:
- `max_flex` limits target flexibility (typically 2-4 residues)
- Prevents combinatorial explosion in K* search
- SCOPE identifies most important contacts
- Volume overlap ranking ensures best contacts are included

### 3. MASTER Database

**Rationale**:
- Provides diverse backbone conformations
- Leverages known protein structures
- Faster than de novo generation
- Can find non-canonical conformations

### 4. K* Evaluation

**Rationale**:
- Validates scaffold binding affinity
- Optimizes scaffold-target interaction
- Filters out non-binding scaffolds
- Provides ranking for ARISE input

## Integration with SCOPE

MONTAGE uses SCOPE in a **simplified way**:

1. **Single amino acid type**: Only VAL (not full 22 AA set)
2. **Quick analysis**: Just for flexibility assignment
3. **No doublets**: Only inter-chain contacts matter
4. **Temporary hulls**: Deleted after use

**Why simplified?**
- Scaffolds are polyalanine (simple)
- Only need to identify flexible target residues
- Full SCOPE analysis happens later in ARISE

## Integration with ARISE

MONTAGE output feeds directly into ARISE:

1. **GMEC PDBs**: Starting structures for ARISE
2. **Ranked by K***: Best scaffolds processed first
3. **Polyalanine**: ARISE adds side chains iteratively
4. **GLY/PRO preserved**: Maintained throughout ARISE

## Performance Considerations

### MASTER Search
- **Time**: ~10 minutes per PDB
- **Matches**: Typically 20-100 per search
- **Bottleneck**: Database size and query complexity

### K* Evaluation
- **Time**: ~1-5 hours per scaffold (on cluster)
- **Scaffolds**: 20-100 per input PDB
- **Bottleneck**: Conformation space size

### Total MONTAGE Time
- **Per PDB**: ~2-10 hours (depending on matches and K* time)
- **Parallelization**: Can process multiple PDBs simultaneously
- **Cluster requirement**: K* runs need cluster access

## Prerequisites

1. **MASTER database**: 
   - Download from https://grigoryanlab.org/master/
   - Configure paths in `resources/db.txt`
   - Requires `master` executable in `resources/`

2. **OSPREY installation**:
   - For K* evaluation
   - Python package via JPype

3. **Cluster access**:
   - SLURM for K* job submission
   - Configure in `resources/montage_cluster_runner.sh`

4. **Input PDBs**:
   - Exactly 2 chains (target + design)
   - Target must be L-space
   - Target chain ID < design chain ID (alphabetically)

## Example Usage

```python
from MONTAGE import run_MONTAGE

# Generate D-peptide scaffolds for L-targets
run_MONTAGE(
    input_pdb_directory="input_pdbs/",  # Directory with 2-chain PDBs
    input_chirality="L",                 # Input design chain chirality
    output_chirality="D",                # Desired output chirality
    MASTER_matches=20,                   # Number of MASTER matches to use
    max_flex=4                           # Max flexible target residues
)
```

## Output Structure

```
<PDB>-MASTER/
  scaffolds/
    match1-MASTER.pdb
    match2-MASTER.pdb
    ...
  match1-MONTAGE/
    kstar-[match1]/
      submit.out          # K* log file
      ensembles/
        seq.*.pdb         # GMEC structures
    ...
  match2-MONTAGE/
    ...

MONTAGE_GMEC/
  match1-complex.pdb      # Best GMEC (K* = 1.23)
  match2-complex.pdb      # Second best (K* = 0.87)
  ...
```

## References

- MONTAGE implementation: `src/main/python/CCKStar/MONTAGE.py`
- MASTER database: https://grigoryanlab.org/master/
- SCOPE integration: `docs/examples/CCKSTAR_SCOPE_ANALYSIS.md`
- ARISE workflow: `docs/examples/CCKSTAR_SCOPE_IN_ARISE.md`
- Full workflow: `docs/examples/CCKSTAR_FULL_WORKFLOW.md`

