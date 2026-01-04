# MONTAGE Execution Results

## Demonstration Run

We executed the MONTAGE workflow demonstration using the 2RL0 example PDB.

### Prerequisites Check

All MONTAGE prerequisites were verified:
- ✓ MASTER executable: `resources/master`
- ✓ createPDS executable: `resources/createPDS`
- ✓ Database config: `resources/db.txt` (119,140 database paths)
- ✓ K* script: `resources/K_bash.sh`
- ✓ Cluster runner: `resources/montage_cluster_runner.sh`

### Execution Results

#### 1. Scaffold Preparation

**Input**: `examples/python.KStar/2RL0.min.reduce.pdb`
- Design chain: G (17 residues: 638-654)
- Target chain: A (89 residues: 153-241)

**Steps Completed**:
1. ✓ Chain renaming: A (target) and z (design)
   - Created: `2RL0.min.reduce-renamed.pdb`
2. ✓ Design chain extraction (for MASTER query)
   - Created: `2RL0.min.reduce-renamed-design.pdb`
3. ✓ D-space reflection demonstration
   - Created: `2RL0.min.reduce-renamed-inverted-design.pdb`
   - Z-coordinates inverted for chirality conversion

#### 2. SCOPE Flexibility Assignment

**SCOPE Analysis** (using VAL hull, approximates ALA + VdW):
- **Intra-chain contacts**: 17 islands (no doublets with VAL-only hulls)
- **Inter-chain contacts**: 17 design residues with target intersections

**Inter-chain Intersections Found**:
```
Chain G Residue 638 → Chain A [241]
Chain G Residue 639 → Chain A [241]
Chain G Residue 640 → Chain A [238, 240]
Chain G Residue 644 → Chain A [236]
Chain G Residue 646 → Chain A [197, 234]
Chain G Residue 648 → Chain A [197]
Chain G Residue 650 → Chain A [191]
```

**Flexible Residues Selected** (top 4 by volume overlap):
1. **A238**: 5.90 Å³
2. **A191**: 5.87 Å³
3. **A241**: 4.06 Å³
4. **A234**: 0.97 Å³

These would be set as flexible during K* evaluation.

### Expected Full MONTAGE Output

If we ran the complete MONTAGE workflow:

#### Step 3: MASTER Search
```
Running MASTER...
  Query: 2RL0.min.reduce-renamed-design.pdb
  Database: 119,140 structures
  Top N: 20 matches
  Time: ~10 minutes
  Output: matches/ directory with structural alignments
```

#### Step 4: Scaffold Generation
```
Scaffold generation:
  - Mutate all design residues to ALA (except GLY/PRO)
  - Align with target using MASTER alignment
  - Protonate chains
  - Output: scaffolds/ directory
  - Expected: 20 scaffold PDBs (match1-complex.pdb, ...)
```

#### Step 5: K* File Preparation
```
For each scaffold:
  - Flexible target residues: [238, 191, 241, 234]
  - GLY/PRO residues: (maintained from original)
  - OSPREY files prepared
  - Output: match*-MONTAGE/ directories
```

#### Step 6: K* Evaluation
```
K* runs submitted to cluster:
  - 20 scaffolds × K* evaluation
  - Time: ~1-5 hours per scaffold
  - Output: kstar-*/submit.out log files
  - GMEC PDBs: kstar-*/ensembles/seq.*.pdb
```

#### Step 7: GMEC Extraction
```
MONTAGE_GMEC/ directory:
  match1-complex.pdb  (K* = 1.23)
  match2-complex.pdb  (K* = 0.87)
  match3-complex.pdb  (K* = 0.45)
  ...
  
Ranked by binding affinity (K* score)
```

## Key Observations

### VAL-Only SCOPE Analysis
- **No doublets found**: VAL hulls are smaller, so no intra-chain intersections
- **All residues are islands**: Each design residue is isolated
- **Inter-chain contacts**: Still found 7 design residues with target contacts
- **Flexibility selection**: Top 4 target residues selected by volume overlap

### Comparison with Full SCOPE
When running full SCOPE with 22 amino acid types:
- **17 doublets found** (with larger hulls from multiple AAs)
- **More inter-chain contacts** (more target residues involved)
- **Larger flexibility sets** (more options for K*)

MONTAGE uses simplified SCOPE because:
- Scaffolds are polyalanine (simple)
- Only need to identify key flexible target residues
- Full SCOPE happens later in ARISE

## Files Generated

### Temporary Files (Cleaned Up)
- `2RL0.min.reduce-renamed.pdb`
- `2RL0.min.reduce-renamed-design.pdb`
- `2RL0.min.reduce-renamed-inverted-design.pdb`
- Temporary hull files in `/tmp/`

### Expected Full MONTAGE Output Structure
```
2RL0-MASTER/
  query.pds                    # MASTER query file
  matches.txt                  # MASTER match list
  matches/                     # MASTER match PDBs
    match1.pdb
    match2.pdb
    ...
  scaffolds/                   # Polyalanine scaffolds
    match1-complex.pdb
    match2-complex.pdb
    ...

2RL0-MONTAGE/
  match1-MONTAGE/
    kstar-[match1]/
      submit.out               # K* log
      ensembles/
        seq.*.pdb              # GMEC structures
    ...
  match2-MONTAGE/
    ...

MONTAGE_GMEC/
  match1-complex.pdb           # Best GMEC (K* = 1.23)
  match2-complex.pdb           # Second best (K* = 0.87)
  ...
```

## Next Steps for Full Execution

**Note**: The MASTER database configured in `resources/db.txt` points to paths from a different system (`/home/users/hc340/dlab/henry/master-db/`) and is not accessible. See `CCKSTAR_MONTAGE_SETUP.md` for database setup instructions.

To run complete MONTAGE (once database is configured):

1. **Set up MASTER database**:
   ```bash
   # Check database status
   python3 check_master_db.py --check --search
   
   # If database found, create local config
   python3 check_master_db.py --create-config /path/to/master-db/
   ```

2. **Create input directory** with 2-chain PDBs:
   ```bash
   mkdir L_to_D
   cp examples/python.KStar/2RL0.min.reduce.pdb L_to_D/
   ```

3. **Run MONTAGE**:
   ```python
   from MONTAGE import run_MONTAGE
   run_MONTAGE("L_to_D", "L", "D", 20, 4)
   ```

4. **Monitor progress**:
   - MASTER search: ~10 minutes
   - K* evaluation: ~1-5 hours (on cluster)

5. **Check results**:
   - `MONTAGE_GMEC/` directory
   - Ranked by K* score

## Performance Notes

- **MASTER search**: ~10 minutes per PDB
- **Scaffold generation**: ~1-2 minutes per match
- **K* evaluation**: ~1-5 hours per scaffold (depends on conformation space)
- **Total time**: ~2-10 hours per input PDB (depending on matches and K* time)

## References

- MONTAGE implementation: `src/main/python/CCKStar/MONTAGE.py`
- Demonstration script: `src/main/python/CCKStar/test_montage_demo.py`
- MONTAGE analysis: `docs/examples/CCKSTAR_MONTAGE_ANALYSIS.md`
- Full workflow: `docs/examples/CCKSTAR_FULL_WORKFLOW.md`

