# SCOPE in ARISE Iterative Design Process

## Overview

ARISE (Affinity-driven Rational Iteration for Sequence Engineering) uses SCOPE in each iteration to guide the selection of which residues to mutate next. This document details how SCOPE is integrated into ARISE's iterative design workflow.

## ARISE Iterative Loop

ARISE follows this recursive process:

```
Round N:
  1. Load GMEC PDBs from previous round (or MONTAGE_GMEC for round 1)
  2. Run SCOPE on current structure → get doublets and inter-chain contacts
  3. Select next doublet to mutate (based on geometry and fixed residues)
  4. Prepare OSPREY K* files for 400 sequences (20×20 AA combinations)
  5. Run K* evaluation → get best sequence and GMEC
  6. Update structure with new GMEC
  7. Repeat Round N+1 until all residues are assigned
```

## Key Function: `doublet_confspace_info()`

**Location**: `KStarPrep.py:36-59`

This function is the bridge between SCOPE and ARISE:

```python
def doublet_confspace_info(pdb_filename, designID, design_muts, design_chirality, outHulls, FixedResidues):
    # 1. Run SCOPE on current structure
    doublet, flex = SCOPE(pdb_filename, outHulls, designID, design_muts, 
                         True, design_chirality, FixedResidues)
    
    # 2. Convert SCOPE output to ConfSpaceSpecs
    all_confspace = []
    for p in doublet:
        if len(p) == 2:  # Doublet (pair of residues)
            doub = list(p)
            # Get flexible target residues for each design residue
            res1_flex = flex[doub[0] - 1]  # flex is 0-indexed list
            res2_flex = flex[doub[1] - 1]
            # Union of flexible residues
            flex_set = list(set(res1_flex).union(res2_flex))
            newspace = ConfSpaceSpecs(doub, flex_set, design_muts, doublet)
            all_confspace.append(newspace)
        elif len(p) == 1:  # Island (single residue)
            doub = list(p)
            res_flex = flex[doub[0] - 1]
            newspace = ConfSpaceSpecs(doub, res_flex, design_muts, doublet)
            all_confspace.append(newspace)
    
    return all_confspace
```

### Key Points

1. **SCOPE is called in each round** with the current GMEC structure
2. **Fixed residues** are passed to SCOPE (residues already assigned in previous rounds)
3. **Output**: List of `ConfSpaceSpecs` objects, one per doublet/island
4. **Flexible residues**: Union of inter-chain contacts for both residues in a doublet

## ARISE Round Preparation: `fileprep_scan_round()`

**Location**: `ARISE.py:251-313`

This function prepares each round of ARISE:

```python
def fileprep_scan_round(new_scan_PDB, visited_doublets, design_ID, target_ID, 
                        out_kstar_files, out_hull_files, finished_matches, 
                        chain_length, design_chirality):
    
    for file in os.listdir(new_scan_PDB):
        # 1. Identify fixed residues (already assigned)
        all_visited_doublets = visited_doublets[curr_matchnum]
        fixed_residues = set()
        for d in all_visited_doublets:
            for res in d:
                fixed_residues.add(res)
        
        # 2. Run SCOPE to get current doublets
        all_specs = doublet_confspace_info(
            file, design_ID, design_chain_muts, design_chirality,
            hull_foldername, list(fixed_residues)
        )
        
        # 3. Choose next doublet to mutate
        selected_spec = choose_next_doublet(
            all_visited_doublets, all_specs, fixed_residues, chain_length
        )
        
        # 4. Reduce flexible target residues (if too many)
        reduced_spec = reduce_doublet(
            design_ID, target_ID, selected_spec, 2, hull_foldername
        )
        
        # 5. Prepare OSPREY K* files
        osprey_fileprep_kstar(...)
```

### Step-by-Step Process

#### Step 1: Identify Fixed Residues
- Extract from `visited_doublets` (residues already mutated in previous rounds)
- These residues are passed to SCOPE as `FixedResidues`
- SCOPE will not create hulls for these (or uses wildtype hulls)

#### Step 2: Run SCOPE
- **Input**: Current GMEC PDB structure
- **Fixed residues**: Already assigned residues
- **Output**: Updated doublets and inter-chain contacts based on current structure

#### Step 3: Select Next Doublet
- Uses `choose_next_doublet()` to pick which 2 residues to mutate next
- Selection strategy (see below)

#### Step 4: Reduce Flexibility
- If too many target residues are flexible (> max_flex), use volume overlap to rank and select top N
- Uses `reduce_doublet()` which calls `rank_flex_overlap()` internally

#### Step 5: Prepare K* Files
- Generate OSPREY configuration files for 400 sequences (20×20 AA combinations)
- Set flexible target residues from SCOPE output

## Doublet Selection Strategy: `choose_next_doublet()`

**Location**: `ARISE.py:188-247`

The algorithm selects the next doublet based on connectivity:

### Priority 1: Share Residue with Previous Doublet
```python
previous_doublet = all_visited_doublets[-1]
for spec in all_specs:
    shared_res = set(previous_doublet) & set(spec.doublet)
    if len(shared_res) == 1 and len(spec.doublet) == 2:
        # Both residues in new doublet are fixed
        if spec.doublet[0] in fixed_residues and spec.doublet[1] in fixed_residues:
            return spec  # Select this doublet
```

**Rationale**: Extend the design by one residue at a time, maintaining connectivity.

### Priority 2: Share Fixed Residue with Any Previous
```python
for spec in all_specs:
    shared_res = fixed_residues & set(spec.doublet)
    if len(shared_res) == 1 and len(spec.doublet) == 2:
        return spec  # Select this doublet
```

**Rationale**: Connect to any previously fixed residue.

### Priority 3: No Fixed Residues (New Region)
```python
for spec in all_specs:
    shared_res = fixed_residues & set(spec.doublet)
    if len(shared_res) == 0 and len(spec.doublet) == 2:
        if spec.doublet[0] not in fixed_residues and spec.doublet[1] not in fixed_residues:
            return spec  # Select this doublet
```

**Rationale**: Start a new design region if all residues are unfixed.

### Priority 4: Islands (Single Residues)
```python
if no_doublet_found:
    # Select an island (single residue with no intra-chain contacts)
    selected_island = random.choice(unfixed_islands)
    # Create new doublet with island + another residue
    return new_doublet
```

**Rationale**: Handle isolated residues that don't form doublets.

## Flexibility Reduction: `reduce_doublet()`

**Location**: `KStarPrep.py:91-145`

If a doublet has too many flexible target residues (> max_flex), this function:

1. **Loads hull files** for design residues and target residues
2. **Calculates volume overlaps** using `find_volume_overlap()`
3. **Ranks by overlap** (largest first)
4. **Selects top N** (max_flex) target residues

This ensures K* calculations remain tractable while focusing on the most important contacts.

## Example: ARISE Round Progression

### Round 1 (Starting from MONTAGE GMEC)
- **Input**: `MONTAGE_GMEC/match1-gmec.pdb` (polyalanine scaffold)
- **Fixed residues**: GLY/PRO only (from MONTAGE)
- **SCOPE**: Finds all doublets in current structure
- **Select**: First doublet (e.g., {1, 2})
- **Mutate**: 400 sequences for residues 1 and 2
- **Best**: Sequence with highest K* score
- **Output**: `round_1/match1-[1_2].pdb` (GMEC with new mutations)

### Round 2
- **Input**: `round_1/match1-[1_2].pdb` (from Round 1)
- **Fixed residues**: {1, 2} (from Round 1)
- **SCOPE**: Recomputes doublets (structure changed!)
- **Select**: Doublet sharing residue with {1, 2} (e.g., {2, 4})
- **Mutate**: 400 sequences for residues 2 and 4 (residue 2 is fixed, so only residue 4 varies)
- **Best**: Sequence with highest K* score
- **Output**: `round_2/match1-[2_4].pdb`

### Round N (Until Complete)
- Continue until all residues are assigned
- Each round uses updated structure from previous round
- SCOPE graph evolves as structure changes

## Why SCOPE is Re-run Each Round

**Critical insight**: The structure changes after each round, so:

1. **Doublets may change**: Residue-residue contacts can appear/disappear
2. **Inter-chain contacts change**: Target flexibility needs updating
3. **New opportunities**: Previously inaccessible contacts may become available

This is why ARISE is **iterative** - it adapts to the evolving structure.

## Integration with K* Evaluation

SCOPE output directly feeds into OSPREY K*:

1. **Doublet selection** → Which 2 residues to mutate
2. **Flexible target residues** → Which target residues to include in K* search
3. **Conformation space** → OSPREY uses this to set up the search space

K* then evaluates 400 sequences (20×20 AA combinations) and returns:
- Best sequence (highest binding affinity)
- GMEC structure (lowest energy conformation)
- Partition functions (for binding affinity calculation)

## Key Data Structures

### `ConfSpaceSpecs`
```python
class ConfSpaceSpecs:
    def __init__(self, doublet, flexset, mutations, graph_path):
        self.doublet = doublet        # [res1, res2] or [res]
        self.flexset = flexset       # List of flexible target residue IDs
        self.mutations = mutations   # Design amino acid types
        self.graph_path = graph_path # All doublets (for reference)
```

### `visited_doublets`
```python
visited_doublets = {
    'match1': [[1, 2], [2, 4], [4, 6]],  # List of doublets assigned
    'match2': [[1, 3], [3, 5]],
    ...
}
```

## Performance Considerations

- **SCOPE overhead**: Re-running SCOPE each round adds computational cost
- **Hull generation**: Can be cached if structure hasn't changed significantly
- **Doublet selection**: O(n) where n = number of doublets
- **Flexibility reduction**: O(m²) where m = number of flexible residues

## References

- ARISE main loop: `ARISE.py:395-442`
- SCOPE integration: `KStarPrep.py:36-59`
- Doublet selection: `ARISE.py:188-247`
- Flexibility reduction: `KStarPrep.py:91-145`

