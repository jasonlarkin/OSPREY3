# CCKStar MONTAGE Execution Progress

## Status: K* Files Prepared, Cluster Submission Skipped

**Date**: December 21, 2024  
**Input**: `2RL0.min.reduce.pdb` (L→D scaffold generation)  
**MASTER Matches**: 10  
**Max Flexible Residues**: 4

## Completed Steps

### 1. MASTER Database Setup ✓

- **Downloaded**: 14,546 .pds files (~4.4 GB) via rsync from `grigoryanlab.org::masterDB/`
- **Location**: `src/main/python/CCKStar/master-db/`
- **Config Created**: `resources/db.txt.local` (14,546 paths)
- **MONTAGE.py Updated**: Changed database path from `./resources/db.txt` to `./resources/db.txt.local`

### 2. MASTER Search ✓

- **Query**: Inverted D-space design chain from 2RL0
- **Database**: 14,546 structures searched
- **Results**: 10 matches found and processed
- **Output**: `matches/` directory with 10 scaffold PDBs

### 3. Scaffold Generation ✓

- **Processed**: All 10 MASTER matches
- **Disjoint Segments**: 0 matches deleted
- **Output**: `scaffolds/` directory with 10 complex PDBs
- **Chirality**: L-target + D-design scaffolds generated

### 4. K* File Preparation ✓

All 10 matches processed through full K* preparation pipeline:

#### Match Processing Status:
- ✓ match1: Confspace compiled, K* files prepared
- ✓ match2: Confspace compiled, K* files prepared
- ✓ match3: Confspace compiled, K* files prepared
- ✓ match4: Confspace compiled, K* files prepared
- ✓ match5: Confspace compiled, K* files prepared
- ✓ match6: Confspace compiled, K* files prepared
- ✓ match7: Confspace compiled, K* files prepared
- ✓ match8: Confspace compiled, K* files prepared
- ✓ match9: Confspace compiled, K* files prepared
- ✓ match10: Confspace compiled, K* files prepared

#### Files Generated Per Match:
Each `match*-MONTAGE/kstar-[MONTAGE]/` directory contains:
- `target.ccsx` - Compiled target confspace
- `design.ccsx` - Compiled design confspace
- `complex.ccsx` - Compiled complex confspace
- `*.confspace` - Uncompiled confspace files
- `*.omol` - OSPREY molecule files
- `*.pdb` - Processed PDB files
- `*sh` - K* execution script

### 5. SCOPE Integration ✓

- **Flexibility Assignment**: SCOPE used to identify flexible target residues
- **Selection Method**: Top 4 residues by volume overlap with design chain hulls
- **Example (match2)**: Selected `['A191', 'A234', 'A158', 'A156']` with overlaps ranging from 10.7 to 0.04 Å³

## Fixes Applied

### 1. Directory Creation Errors
**Problem**: `FileExistsError` when directories already existed from previous runs  
**Fix**: Added checks to remove existing directories before creating new ones in:
- `cleanup_MASTER()` - Removes existing `*-MASTER` directories
- `scaffold_generator()` - Removes existing `scaffolds/` directory
- `run_MONTAGE()` - Removes existing `*-MONTAGE` directories
- `get_MONTAGE_gmec()` - Removes existing `*-MONTAGE_GMEC` directories

### 2. OMOL Parsing Error
**Problem**: `IndexError` when parsing residue IDs from OMOL files  
**Location**: `KStarPrep.py:get_omol_res_chain()`  
**Fix**: 
- Extract residue number from format like `'A241'` → `'241'`
- Made parsing robust to handle both quoted and unquoted formats
- Updated `make_target_confspace()` to properly handle chain+residue strings

### 3. Cluster Submission Error
**Problem**: `sbatch` command not available on local machine  
**Fix**: 
- Added check for `sbatch` availability
- Skip cluster submission if not available
- Print informative message about manual K* execution
- K* files remain prepared and ready for execution

## Current State

### Output Directories

```
src/main/python/CCKStar/
├── 2RL0-MASTER/              # MASTER search outputs
│   ├── query.pds
│   ├── matches.txt
│   ├── matches/              # 10 MASTER match PDBs
│   └── scaffolds/            # 10 scaffold complex PDBs
│
├── 2RL0-MONTAGE/             # K* preparation outputs
│   ├── match1-MONTAGE/
│   │   └── kstar-[MONTAGE]/  # K* files ready for execution
│   ├── match2-MONTAGE/
│   │   └── kstar-[MONTAGE]/
│   └── ... (matches 3-10)
│
└── input_pdbs/
    └── 2RL0.min.reduce.pdb
```

### K* Execution Status

**Status**: Files prepared, not yet executed  
**Reason**: Cluster submission skipped (no `sbatch` available)  
**Next Step**: Execute K* scripts manually or configure cluster access

## Next Steps

### Option 1: Local K* Execution (Recommended for No Cluster)

**Prerequisites**: Build osprey runtime first:
```bash
cd /path/to/osprey-fork_modern
./gradlew runtime
```

This creates the osprey executable at `build/image/bin/osprey`.

**Run K* locally**:
```bash
cd src/main/python/CCKStar
./run_kstar_local.sh
```

Or using Python script:
```bash
python3 run_kstar_local.py
```

The scripts will:
- Automatically find the osprey binary
- Process all matches sequentially
- Skip already completed calculations
- Save output to `submit.out` in each kstar directory

### Option 2: Manual K* Execution

Run K* scripts manually from each match directory:

```bash
cd src/main/python/CCKStar
for match_dir in 2RL0-MONTAGE/match*-MONTAGE/kstar-*/; do
    cd "$match_dir"
    # Update the osprey3 path in K_bash.sh, then:
    bash K_bash.sh  # Execute the K* script
    cd ../../../../..
done
```

### Option 3: Configure Cluster Access

If you have access to a SLURM cluster:
1. Ensure `sbatch` is available in PATH
2. Rerun MONTAGE or manually submit jobs:
   ```bash
   cd src/main/python/CCKStar
   bash resources/montage_cluster_runner.sh
   ```

### Option 3: Extract GMEC PDBs (If K* Already Run)

If K* has been executed (manually or via cluster):

```python
from MONTAGE import get_MONTAGE_gmec
get_MONTAGE_gmec('2RL0')
```

This extracts the GMEC (Global Minimum Energy Conformation) PDBs for use in ARISE.

## Performance Notes

- **MASTER Search**: ~1-2 minutes for 10 matches
- **Scaffold Generation**: ~1 minute for 10 scaffolds
- **K* File Preparation**: ~2-3 hours total for 10 matches
  - SCOPE analysis: ~30 seconds per match
  - Confspace compilation: 3-15 minutes per match (varies by search space size)
- **Total Time**: ~3-4 hours for full MONTAGE workflow (excluding K* execution)

## Files Modified

1. `MONTAGE.py`:
   - Updated database path to `db.txt.local`
   - Fixed directory creation to handle existing directories
   - Added cluster submission check for `sbatch` availability

2. `KStarPrep.py`:
   - Fixed OMOL parsing to handle chain+residue format (`'A241'` → `'241'`)
   - Made parsing robust for quoted/unquoted formats

3. `check_master_db.py`:
   - Fixed recursive search for .pds files in subdirectories

## Database Information

- **Source**: grigoryanlab.org::masterDB/
- **Download Method**: rsync
- **Total Files**: 14,546 .pds files
- **Total Size**: ~4.4 GB
- **Config File**: `resources/db.txt.local`
- **Database Date**: October 22, 2014 (as per MASTER website)

## Workflow Summary

```
Input PDB (2RL0.min.reduce.pdb)
    ↓
Chain Renaming (A/z)
    ↓
D-space Reflection (L→D)
    ↓
MASTER Search (14,546 structures)
    ↓
10 Matches Found
    ↓
Scaffold Generation (10 complexes)
    ↓
SCOPE Flexibility Assignment (4 residues/match)
    ↓
K* File Preparation (10 matches)
    ↓
[Cluster Submission - SKIPPED]
    ↓
K* Files Ready for Manual Execution
```

## Known Issues

1. **Cluster Submission**: Requires SLURM cluster with `sbatch` command (now optional with local execution scripts)
2. **K* Execution**: Can be run locally using `run_kstar_local.sh` or `run_kstar_local.py` after building osprey3
3. **GMEC Extraction**: Requires completed K* runs
4. **osprey Binary**: Must be built using `./gradlew runtime` before local K* execution (creates `build/image/bin/osprey`)

## Documentation References

- `CCKSTAR_MONTAGE_ANALYSIS.md` - Algorithm details
- `CCKSTAR_MONTAGE_SETUP.md` - Setup instructions
- `CCKSTAR_MONTAGE_EXECUTION.md` - Execution details
- `CCKSTAR_MONTAGE_DATABASE_STATUS.md` - Database status

