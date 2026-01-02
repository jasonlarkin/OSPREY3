# Testing ARISE with Example Results

## Quick Start: Single Match

**Step 1**: Convert example results (already done)
```bash
cd src/main/python/CCKStar
python3 convert_example_results_for_arise.py --match-name match1
```

**Step 2**: Run ARISE with single match
- ARISE's `are_kstar_finished()` checks for "completed" in submit.out (now included)
- ARISE processes matches in `2RL0-MONTAGE/match*-MONTAGE/`
- Currently only `match1-MONTAGE` exists

**Note**: ARISE may need modification to work with single match, or you can test the workflow manually.

## For Performance Profiling: Multiple Matches

**Step 1**: Duplicate the converted match N times
```bash
cd src/main/python/CCKStar
python3 duplicate_match_for_profiling.py \
    --source-match 2RL0-MONTAGE/match1-MONTAGE \
    --num-copies 9 \
    --target-dir 2RL0-MONTAGE
```

This creates: `match2-MONTAGE`, `match3-MONTAGE`, ..., `match10-MONTAGE` (all identical copies)

**Step 2**: Run ARISE with multiple matches
- ARISE will process all 9 matches
- Good for profiling parallel processing, memory usage, etc.

## What Was Converted

**Source**: `examples/python.KStar/kstar.results.tsv`
- 6 sequences with K* scores (log10: 11-15)
- Tab-separated format

**Output**: `2RL0-MONTAGE/match1-MONTAGE/kstar-[MONTAGE]/submit.out`
- Comma-separated format with "Assignments" header
- Fields: seq_num, mutations, K* score, zeros, ligand pfunc values
- "completed" marker added for ARISE's completion check
- Ensemble PDBs copied to `ensembles/` directory

## ARISE Requirements Met

- [x] "Assignments" header in submit.out
- [x] Comma-separated format
- [x] Field 1: Mutations (G649-phe format)
- [x] Field 2: K* score (positive log10 values)
- [x] Field 9: Mutant ligand pfunc
- [x] Field 10: WT ligand pfunc
- [x] "completed" marker for are_kstar_finished()
- [x] Ensemble PDBs in ensembles/ directory

## Testing Workflow

1. **Single match test**: Verify ARISE can read and process the converted format
2. **Multiple matches**: Duplicate for profiling to see how ARISE scales
3. **Real K* results**: Later run actual K* with relaxed settings (epsilon=0.99) for more realistic data

## Notes

- All duplicated matches are identical (same sequences, same scores)
- For realistic profiling, you'd want varied matches, but identical matches are fine for:
  - Testing ARISE's parallel processing
  - Memory profiling
  - Workflow validation
  - Performance baseline

- For production, you'd run actual K* on each unique match (30-40 hours each)

