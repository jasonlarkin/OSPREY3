# CCKStar Integration Notes

## Overview

This document provides technical details about how CCKStar integrates with OSPREY's Java K* algorithm and how it can be used with an external DSL/compiler pipeline.

## Python-Java Integration

CCKStar uses **JPype** to interface with OSPREY's Java code:

```python
import jpype
if not jpype.isJVMStarted():
    import osprey
    osprey.start()
import osprey.prep
```

The `osprey` Python package provides a bridge to OSPREY's Java classes, allowing Python scripts to:
- Prepare OSPREY configuration files
- Set up conformation spaces
- Invoke K* calculations
- Parse K* results

## Key Integration Points

### 1. SCOPE → OSPREY Configuration

SCOPE output (doublets and inter-chain contacts) is used to prepare OSPREY files:

```python
from KStarPrep import osprey_fileprep_kstar, ConfSpaceSpecs

# SCOPE identifies doublets and flexible residues
intrachain_pairs, interchain_pairs = SCOPE(...)

# Convert to OSPREY configuration
confspace_specs = [
    ConfSpaceSpecs(
        doublet=[res1, res2],      # From SCOPE intrachain_pairs
        flexset=flexible_residues, # From SCOPE interchain_pairs
        mutations=design_aa_types,
        graph_path=intrachain_pairs
    )
]

# Prepare OSPREY files
osprey_fileprep_kstar(
    pdb_input,
    out_directory,
    design_mutations,
    confspace_specs,
    design_chain_id,
    design_chirality,
    ...
)
```

### 2. K* Execution

K* is executed via shell scripts (typically on a cluster):

```bash
# resources/K_bash.sh
# Runs OSPREY K* Java application
java -cp ... edu.duke.cs.osprey.kstar.KStar ...
```

The Python code monitors K* execution and parses results from output files.

### 3. ARISE Iterative Loop

ARISE uses SCOPE in each iteration:

1. Start with MONTAGE GMEC PDB
2. Run SCOPE to get current doublets and contacts
3. Select next 2 residues to mutate (from SCOPE graph)
4. Prepare OSPREY files for 400 sequences (20×20 AA combinations)
5. Run K* for each sequence
6. Get GMEC of best sequence
7. Repeat with updated structure

## Residue ID Mapping Issue

**Important**: SCOPE uses 1-indexed positions in doublets, but PDB files use actual residue IDs. This affects:

- File naming: `ChainGRes638.pdb` vs position `1`
- Ranking functions: `rank_flex_overlap()` expects positions but files use IDs
- OSPREY integration: Must map between positions and IDs

**Solution**: Extract mapping from hull files or modify functions to accept mappings.

See `docs/examples/CCKSTAR_SCOPE_ANALYSIS.md` for details.

## For DSL Compiler Integration

### Input Requirements

The DSL compiler should generate:

1. **PDB files**: 2-chain structures (target + design)
2. **Design parameters**:
   - Design chain ID
   - Target chain ID
   - Amino acid types to consider
   - Chirality (L or D)
   - Fixed residues (if any)

3. **MONTAGE parameters** (if using):
   - MASTER database paths
   - Number of matches
   - Max flexible residues

4. **ARISE parameters** (if using):
   - Design chain length
   - Apo tolerance
   - Starting round (if resuming)

### Output Format

CCKStar produces:

1. **SCOPE output**:
   - Doublets (JSON/dict): `[{1, 2}, {2, 4}, ...]`
   - Inter-chain contacts (list): `[[223, 240, 241], [221, 241], ...]`
   - Hull PDB files: `ChainGRes638.pdb`, etc.

2. **MONTAGE output**:
   - Scaffold PDBs: `match1-MONTAGE/`, etc.
   - GMEC PDBs: `MONTAGE_GMEC/match1-gmec.pdb`

3. **ARISE output**:
   - Round directories: `round_1/`, `round_2/`, etc.
   - Final designs: `completed-matches/match1-final.pdb`
   - K* results: Partition functions, binding affinities

### Integration Points

1. **SCOPE Analysis**:
   ```python
   # DSL compiler generates this call
   intrachain_pairs, interchain_pairs = SCOPE(
       pdb_file,
       output_folder,
       design_chain_id,    # From DSL
       design_aa_types,    # From DSL
       save_pdb=True,
       design_chirality,  # From DSL
       fixed_residues      # From DSL
   )
   ```

2. **K* File Preparation**:
   ```python
   # Convert SCOPE output to OSPREY format
   confspace_specs = prepare_confspace_specs(
       intrachain_pairs,
       interchain_pairs,
       design_aa_types
   )
   
   # Generate OSPREY config files
   osprey_fileprep_kstar(...)
   ```

3. **Result Parsing**:
   ```python
   # Parse K* output files
   kstar_results = parse_kstar_output(kstar_directory)
   # Extract: partition functions, binding affinities, GMEC PDBs
   ```

## File Locations

- **SCOPE**: `src/main/python/CCKStar/Find_Doublets.py`
- **MONTAGE**: `src/main/python/CCKStar/MONTAGE.py`
- **ARISE**: `src/main/python/CCKStar/ARISE.py`
- **K* Prep**: `src/main/python/CCKStar/KStarPrep.py`
- **OSPREY Bridge**: `src/main/python/osprey/` (JPype interface)

## Dependencies

- **Python**: PyVista, BioPython, scipy, numpy
- **Java**: OSPREY JAR files (via JPype)
- **External**: MASTER database (for MONTAGE)

## Next Steps for DSL Compiler

1. **Generate SCOPE calls** from DSL experiment definitions
2. **Map DSL residue selections** to SCOPE doublets
3. **Generate OSPREY config files** from SCOPE output
4. **Parse K* results** and format for DSL output
5. **Handle residue ID mappings** between DSL, SCOPE, and OSPREY

## References

- CCKStar Full Workflow: `docs/examples/CCKSTAR_FULL_WORKFLOW.md`
- SCOPE Analysis: `docs/examples/CCKSTAR_SCOPE_ANALYSIS.md`
- OSPREY Pipeline: `docs/examples/OSPREY_PIPELINE_EXAMPLES.md`
- K* Performance: `docs/performance/KSTAR_PERFORMANCE_ANALYSIS.md`

