# MASTER Algorithm and Data Structures

## Primary Data Structures

### 1. CenResGrp (Center Residue Groups)

**Purpose**: Tracks search state for each query segment during backtracking.

**Structure**:
```cpp
class CenResGrp {
    vector<CenterResidue> _cres;  // One CenterResidue per query segment
};
```

**CenterResidue** contains per-segment state:
- `vector<int> matchCand`: Candidate target residues (from L1/L2 filtering)
- `int curMatchCandIdx`: Current candidate being explored (backtracking state)
- `vector<int> tsResFlag`: Per-target-residue flags (NOT_MATCH, MATCH_L1, MATCH_L2)
- `double rank`: Match candidate density (candidates / segment_length)
- Segment boundaries: `begRes`, `endRes`, `resIdx`, `segLen`
- RMSD cutoffs: `selfRmsdCut`, `sofarRmsdCut`, `curDistDevCut`

**Key Property**: Search processes segments in rank order (highest density first).

### 2. TargetStruct Distribution Tables (Primary Lookup Structure)

**Purpose**: Pre-computed lookup tables for fast candidate filtering.

**Distance Distribution** (`_distdistr`):
```cpp
vector<vector<vector<int>>> _distdistr;
// [res_i][res_j][distance_bin] → list of target residues
```

**Dihedral Distribution** (`_diheddistr`):
```cpp
vector<vector<vector<int>>> _diheddistr;
// [res_i][phi_bin][psi_bin] → list of target residues
```

**Lookup Pattern**:
1. Given query residues i, j and distance d → bin distance
2. `_distdistr[i][j][bin]` → O(1) list of candidate target residues
3. Same for dihedral angles (phi, psi)

**Memory Complexity**: O(N² × bins) where N = residues, bins = distance/angle bins

**Trade-off**: Large memory footprint (pre-computed) enables fast filtering.

### 3. QueryStruct

**Purpose**: Represents query structure with segments.

**Key Data**:
- `vector<int> _cenres`: Center residue index for each segment
- `vector<vector<double>> _crdihed`: Phi/psi angles for center residues
- `vector<int> _befbrk`: Segment boundaries (residues before break)
- `vector<vector<double>> _bbcoor`: Backbone coordinates (from ProteinStruct)

### 4. ProteinStruct Base Class

**Purpose**: Common structure representation.

**Key Data**:
- `vector<vector<double>> _bbcoor`: Backbone coordinates [residue][atom] → (x,y,z)
- `vector<vector<double>> _cacoor`: CA coordinates
- `map<pair<int, int>, double> _dist`: Distance cache
- `Structure _sys`: Full structure hierarchy (Structure → Chain → Residue → Atom)

## Algorithm: Multi-Level Pruning with Backtracking

### Overview

MASTER uses a **backtracking search** with **progressive multi-level pruning**:

```
searchByDistDistr()
  ├── For each target structure in database (sequential):
  │     ├── Initialize CenResGrp for query segments
  │     ├── L1 Filter: Dihedral angle lookup → candidate residues
  │     ├── Rank segments by candidate density
  │     └── auxSearchByDistDistrBB() [backtracking]
  │           ├── Segment 0: Try all L1 candidates
  │           ├── Segment 1: Filter by distance deviation + L2
  │           ├── Segment 2: Full RMSD check
  │           └── If all segments match → add to results
  └── Sort and output matches
```

### Algorithm Steps

#### Step 1: L1 Filtering (Dihedral Angle Pruning)

```cpp
// For each query segment center residue:
ts.readDihedDistr(cres[i].getMatchCand(), 
                  cres[i].getPhi(), cres[i].getPsi(), 
                  phieps, psieps);
// Returns: vector<int> of candidate target residues with matching phi/psi
```

**Operation**: O(1) lookup in `_diheddistr[res_i][phi_bin][psi_bin]`

#### Step 2: Ranking

```cpp
cres[i].setRank(float(cres[i].getMatchCand().size()) / 
                float(cres[i].getSegLen()));
cres.sortByRank();  // Process highest-density segments first
```

**Rationale**: Segments with fewer candidates are more constrained → process first to fail fast.

#### Step 3: Backtracking Search (`auxSearchByDistDistrBB`)

**Algorithm**: Iterative backtracking (not recursive stack-based)

```cpp
cri = 0;  // Current segment index
while (1) {
    if (cri < 0) break;  // All segments processed
    
    // Try next candidate for current segment
    for (c = cres[cri].getCurMatchCandIdx() + 1; 
         c < cres[cri].getMatchCand().size(); c++) {
        
        // L2 Filter: Distance deviation check
        qd = qs.calcDistBB(segment_i-1, segment_i);
        td = ts.calcDistBB(match_i-1, match_candidate);
        if (fabs(td - qd) > distDevCut) continue;
        
        // Overlap check (segments must not overlap)
        if (overlaps_previous_segments()) continue;
        
        // Gap length constraint
        if (gap_len_violated()) continue;
        
        // If last segment: Full RMSD calculation
        if (cri == num_segments - 1) {
            rmsd = calcRmsdKabsch(query_coords, target_coords);
            if (rmsd <= cutoff) {
                mlist.addMatch(m);
            }
        } else {
            // Advance to next segment
            cri++;
        }
    }
    
    // Backtrack: move to previous segment
    cri--;
}
```

**Key Features**:
- **Progressive RMSD cutoff**: Tighter as more segments match
- **Distance deviation pruning**: Rejects candidates with wrong inter-segment distances
- **Gap constraints**: Enforces gap lengths between segments
- **Overlap prevention**: Ensures segments don't overlap in target

#### Step 4: Full RMSD (Kabsch Algorithm)

Only computed for complete matches (all segments placed):

```cpp
rmsd = calcRmsdKabsch(query_backbone_coords, target_backbone_coords);
```

**Complexity**: O(N) where N = number of backbone atoms

**Optimization**: Only called after all other filters pass.

### Algorithm Complexity

**Time Complexity**:
- **L1 Filtering**: O(S × N) where S = segments, N = target residues
- **Backtracking**: O(S × C^S) worst-case, but heavily pruned by:
  - L1 dihedral filtering (reduces candidates by ~100-1000x)
  - L2 distance filtering (reduces by ~10-100x)
  - Progressive RMSD cutoff (early termination)
- **RMSD Calculation**: O(S × atoms_per_segment) per complete match

**Space Complexity**:
- **Distribution tables**: O(N² × bins) per target structure
- **Search state**: O(S × candidates) for CenResGrp
- **Matches**: O(M) where M = number of matches found

**Typical Performance**: ~12 seconds per query against 14,546 target structures (from profiling).

## Parallelism

### Current State: **NONE**

**Evidence from code**:
- No `#pragma omp` directives
- No `std::thread` usage
- No MPI code
- Sequential loop over target structures:
  ```cpp
  for (tsi = 0; tsi < mlist.getNumTs(); tsi++) {
      auxSearchByDistDistrBB(mlist, cres, qs, ts, tsi, ...);
  }
  ```

### Parallelism Opportunities

#### 1. Target Structure Parallelism (Embarrassingly Parallel)

**Opportunity**: Search multiple target structures in parallel.

```cpp
// Current (sequential):
for (tsi = 0; tsi < mlist.getNumTs(); tsi++) {
    search_target(tsi);
}

// Parallel (OpenMP):
#pragma omp parallel for
for (tsi = 0; tsi < mlist.getNumTs(); tsi++) {
    search_target(tsi);  // Independent searches
}
```

**Expected Speedup**: Near-linear (14,546 targets → 4-8x with 4-8 cores)

**Challenges**:
- Thread-safe match list insertion (needs mutex or per-thread aggregation)
- Memory: Each thread needs TargetStruct in memory (large distribution tables)

#### 2. Segment Candidate Parallelism (Limited)

**Opportunity**: Parallel candidate evaluation within a segment.

**Challenges**:
- Backtracking algorithm structure (sequential state)
- State sharing between candidates (current match state)
- **Not recommended**: Algorithm structure doesn't map well to parallel candidates

#### 3. Database Parallelism (MPI/Process-Level)

**Opportunity**: Distribute target database across MPI ranks.

```cpp
// Rank 0: Distribute target indices
// Rank i: Search targets [start_i, end_i]
// Gather results at end
```

**Expected Speedup**: Near-linear (scales with number of ranks/nodes)

**Challenges**:
- Database distribution (large distribution tables)
- Result aggregation
- Load balancing (targets may have varying search times)

### Parallelism Recommendations

**For OSPREY Integration**:
1. **Target-level parallelism** (OpenMP): Easy win, 4-8x speedup
2. **MPI multi-process**: For large databases, scale across nodes
3. **Library integration**: Enables better parallelization than subprocess calls

**Priority**: Low (MASTER is 2.9% of pipeline time). Focus optimization on ConfSpace compilation (80%) and K* (hours).

## Summary

**Primary Data Structure**: **Distribution lookup tables** (`_distdistr`, `_diheddistr`) in TargetStruct
- Pre-computed O(1) lookups enable fast candidate filtering
- Large memory footprint (O(N² × bins)) for fast search

**Algorithm**: **Backtracking search with multi-level pruning**
- L1: Dihedral angle filtering (O(1) lookup)
- L2: Distance deviation filtering
- L3: Full RMSD calculation (only for complete matches)
- Progressive cutoff tightening

**Parallelism**: **None currently**
- Opportunity: Target-level parallelism (embarrassingly parallel)
- Expected speedup: 4-8x with OpenMP
- Priority: Low (not a bottleneck)

