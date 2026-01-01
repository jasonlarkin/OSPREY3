# MASTER Prerequisites for MONTAGE

## Overview

MASTER (Motif Alignment & Search Tool) is a structural search tool from the Grigoryan Lab (https://grigoryanlab.org/master/) used by MONTAGE to find structural matches for peptide scaffolds.

## Required Components

### 1. MASTER Executables

**Location:** `src/main/python/CCKStar/resources/`

- **`master`** (3.2M) - Main MASTER search executable
  - Used to search database for structural matches
  - Command: `./resources/master --query query.pds --targetList <db.txt> ...`
  
- **`createPDS`** (3.2M) - Protein Data Structure converter
  - Converts PDB files to PDS format (MASTER's native format)
  - Command: `./resources/createPDS --type query --pdb <input.pdb> --pds query.pds`

**Status:** Both executables are present and executable

### 2. MASTER Database

**Location:** `src/main/python/CCKStar/master-db/`

- Contains 14,546+ .pds files (Protein Data Structure format)
- Organized in subdirectories (e.g., `55/`, `6v/`, `a0/`, etc.)
- Each .pds file represents a protein structure in MASTER's format
- Database size: ~1.1M+ (directory listing shows many subdirectories)

**Status:** Database directory exists with structure files

### 3. Database Configuration File

**Location:** `src/main/python/CCKStar/resources/db.txt.local`

- Contains absolute paths to all .pds files in the database
- Format: One path per line
- Example entries:
  ```
  $REPO_ROOT/src/main/python/CCKStar/master-db/55/155c_A.pds
  $REPO_ROOT/src/main/python/CCKStar/master-db/6v/16vp_A.pds
  ...
  ```

**Status:** Configuration file exists with 14,527+ database paths

### 4. Supporting Scripts

**Location:** `src/main/python/CCKStar/resources/`

- **`K_bash.sh`** - K* execution script
- **`montage_cluster_runner.sh`** - SLURM cluster submission script (optional)

**Status:** Scripts are present

## How MONTAGE Uses MASTER

### Workflow

1. **Query Preparation:**
   - Extract design chain from input PDB
   - Convert to PDS format: `createPDS --type query --pdb <query.pdb> --pds query.pds`

2. **MASTER Search:**
   - Run: `master --query query.pds --targetList db.txt.local --rmsdCut 10.0 --topN <N> --outType match --seqOut matches.txt --structOut matches`
   - Searches database for structural matches
   - Returns top N matches (typically 10-100)
   - Output: PDB files in `matches/` directory

3. **Scaffold Generation:**
   - For each match, create polyalanine scaffold
   - Align match to target structure
   - Generate complex PDBs

4. **K* Evaluation:**
   - Run K* on each scaffold
   - Rank by designability (K* score)

### Code Location

**File:** `src/main/python/CCKStar/MONTAGE.py`

- `submit_MASTER()` (lines 245-271) - Runs MASTER search
- `run_MASTER()` (lines 274-308) - Orchestrates MASTER workflow
- `scaffold_generator()` (lines 173-242) - Generates scaffolds from matches

## Prerequisites Check

The test script (`test_montage_demo.py`) checks prerequisites by looking for:

```python
prerequisites = {
    'MASTER executable': os.path.exists('resources/master'),
    'createPDS executable': os.path.exists('resources/createPDS'),
    'Database config': os.path.exists('resources/db.txt'),
    'K* script': os.path.exists('resources/K_bash.sh'),
    'Cluster runner': os.path.exists('resources/montage_cluster_runner.sh'),
}
```

**Note:** The check uses relative paths from the script's execution directory. The actual files are in `src/main/python/CCKStar/resources/`, so the check may fail if run from the wrong directory.

## Current Status

All MASTER prerequisites are **present and configured**:

- MASTER executable: Present (3.2M, executable)
- createPDS executable: Present (3.2M, executable)
- Database files: Present (14,546+ .pds files in master-db/)
- Database config: Present (db.txt.local with 14,527+ paths)
- K* script: Present
- Cluster runner: Present

## Running Full MONTAGE

To run full MONTAGE (including MASTER search):

1. **Ensure you're in the correct directory:**
   ```bash
   cd src/main/python/CCKStar
   ```

2. **Run MONTAGE:**
   ```python
   from MONTAGE import run_MONTAGE
   
   run_MONTAGE(
       input_pdb_directory='input_pdbs/',
       input_chirality='L',
       output_chirality='D',
       MASTER_matches=10,  # Number of MASTER matches to use
       max_flex=4           # Max flexible target residues
   )
   ```

3. **Expected Runtime:**
   - MASTER search: ~10 minutes per query (from code comment)
   - Scaffold generation: 2-10 minutes per match
   - K* evaluation: 5-30 minutes per match
   - **Total: 5-30 minutes per match** (matches expected from docs)

## Profiling Full MONTAGE

To profile full MONTAGE including MASTER:

1. Use a test PDB with 2 chains (target + design)
2. Run from `src/main/python/CCKStar/` directory
3. Ensure `resources/master` and `resources/createPDS` are executable
4. Ensure `resources/db.txt.local` points to valid database paths
5. Run profiling script with full MONTAGE workflow

**Note:** MASTER search is computationally expensive (~10 minutes per query), so full profiling will take significantly longer than the demo (which skipped MASTER).

## Troubleshooting

### MASTER executable not found
- Check path: `src/main/python/CCKStar/resources/master`
- Ensure executable permissions: `chmod +x resources/master`

### Database not found
- Check `resources/db.txt.local` contains valid paths
- Verify `master-db/` directory exists with .pds files
- Run: `python3 check_master_db.py --check`

### MASTER search fails
- Verify database paths in `db.txt.local` are absolute and accessible
- Check MASTER executable version: `./resources/master --version` (if supported)
- Review MASTER documentation: https://grigoryanlab.org/master/

## References

- MASTER Website: https://grigoryanlab.org/master/
- MONTAGE README: `src/main/python/CCKStar/README.md`
- Setup Guide: `src/main/python/CCKStar/setup_master_database.md`

