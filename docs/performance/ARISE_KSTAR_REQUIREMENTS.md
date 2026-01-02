# ARISE K* Requirements

## What ARISE Needs from K*

ARISE's `find_best_log()` function reads from `submit.out` and expects:

1. **Header line**: Contains "Assignments"
2. **Data lines**: Comma-separated format with fields:
   - Field 1: Mutations (sequence assignments)
   - Field 2: K* score (must be numeric, positive)
   - Field 9: Mutant partition function (pfunc_mut)
   - Field 10: WT apo ligand partition function (apo_lig_WT)

3. **Minimum requirement**: At least one sequence with a positive K* score per match

## Current K* Output Format

**submit.out** contains:
- Progress lines: `[12 1 22 32 ...] scores: 124848, confs: 1920, score:-1393.440812, ...`
- Space-separated, not comma-separated
- No "Assignments" header
- Format: `[sequence] scores: count, confs: count, score: value, energy: value, bounds:[lower, upper], delta: value, time: value`

**kstar.results.tsv** contains:
- Tab-separated format
- Header: `Seq ID	Sequence	K* Lower Bound	K* Upper Bound	...`
- Only header exists currently (no data rows)

## The Problem

ARISE expects a comma-separated format with "Assignments" header that doesn't exist in current output. This suggests:

1. **K* must complete convergence** (delta <= epsilon) before writing final results
2. **Final results format** may differ from progress format
3. **ARISE may be outdated** and needs to read current format

## What Needs to Happen

### Option 1: K* Must Complete Convergence

K* needs to run until:
- Delta <= epsilon (typically 0.01)
- Final results written to submit.out in expected format
- OR kstar.results.tsv populated with data rows

**Estimated time**: 30-40 hours per match (from current convergence rate)

### Option 2: Modify ARISE to Read Current Format

Update `find_best_log()` to:
- Parse current progress format from submit.out
- Extract sequences and scores from progress lines
- Handle space-separated format instead of comma-separated

**Current progress lines contain**:
- Sequences: `[12 1 22 32 ...]` (rotamer assignments)
- Scores: `score:-1393.440812` (K* scores, but all negative in current run)
- These scores are negative, so ARISE's `score > best_score` check (starting at 0) won't find them

### Option 3: Use kstar.results.tsv Instead

Modify ARISE to read from `kstar.results.tsv`:
- Tab-separated format
- Has proper header
- Would need data rows (currently only header exists)

## Answer: How Much K* Needs to Run

**Short answer**: K* needs to **complete convergence** (delta <= epsilon) for at least one match to produce usable results for ARISE.

**Current state**:
- match1: Delta = 1.000000 (needs <= 0.01)
- Estimated remaining: 30-40 hours per match
- No sequences have positive scores yet (all negative)

**Minimum for ARISE**:
- At least one match must complete convergence
- At least one sequence must have positive K* score
- Results must be written in format ARISE can read

**Practical approach**:
1. Let K* complete convergence on at least one match (30-40 hours)
2. Verify results format matches ARISE expectations
3. If format mismatch, either:
   - Update ARISE to read current format
   - Or ensure K* writes in expected format upon completion

## Current Progress Analysis

From match1 submit.out:
- 124,848 sequences evaluated
- 1,920 conformations explored
- All scores negative (best: -1393.440812)
- Bounds: [985.32, 1023.19] log10p1
- Delta: 1.000000 (no convergence)

**Conclusion**: Current partial results are **not sufficient** for ARISE. K* must complete convergence to produce final results in the expected format.

