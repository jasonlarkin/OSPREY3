# MASTER Code Analysis

## Executive Summary

MASTER (Method of Accelerated Search for Tertiary Ensemble Representatives) is a C++ structural similarity search tool used in OSPREY's MONTAGE stage. It accounts for **2.9% of pipeline time** (~12 seconds per match) and is **not a significant bottleneck**.

**Key Finding**: MASTER is already in C++, eliminating the need for language translation. Performance is adequate for current use, but library integration (vs. subprocess) would reduce overhead.

## Performance Profile

### Runtime Characteristics

From OSPREY profiling data:
- **Per-match time**: ~12 seconds per match (63s for 5 matches)
- **Pipeline percentage**: 2.9% of total time (63s out of 2191s)
- **Status**: Not a bottleneck

### Performance Context

**Compared to other pipeline stages**:
- SCOPE: 10-30 seconds (1-5 min total)
- MONTAGE scaffold generation: 2-10 minutes per match
- ConfSpace compilation: 10-60 seconds (80% of MONTAGE time)
- K* execution: 1-3 hours per sequence

**Conclusion**: MASTER execution time is negligible compared to downstream stages (ConfSpace compilation, K*). Optimization priority should be elsewhere.

## Code Statistics

**Source Code Metrics**:
- **Total files**: 33 (C++ source + headers)
- **Lines of code**: 8,954 lines (C++ implementation)
- **Lines of headers**: 1,760 lines
- **Total**: ~10,714 lines

**Largest Components**:
- `src/Search.cpp`: 1,647 lines (main search algorithm)
- `src/msttypes.cpp`: 1,482 lines (core data structures)
- `src/ProteinStruct.cpp`: 1,104 lines (protein structure handling)
- `src/Redundancy.cpp`: 1,051 lines (redundancy removal)
- `src/TargetStruct.cpp`: 601 lines (target structure handling)
- `src/Common.cpp`: 501 lines (common utilities)
- `src/QueryStruct.cpp`: 385 lines (query structure handling)
- `programs/master.cpp`: 330 lines (CLI entry point)
- `include/msttypes.h`: 554 lines (core type definitions)

**Code Complexity**: Moderate - well-structured C++ with clear class hierarchy.

## Algorithm Overview

MASTER performs structural similarity searches using a **multi-level pruning strategy**:

### Core Algorithm

1. **PDS Format** (Protein Data Structure):
   - Pre-computed binary format for fast lookups
   - Stores backbone coordinates, dihedral angles, distance distributions
   - Enables rapid filtering before expensive RMSD calculations

2. **Multi-Level Search**:
   - **L1 Match**: Dihedral angle filter (phi/psi within tolerance)
   - **L2 Match**: Distance distribution filter
   - **L3 Match**: Full RMSD calculation (Kabsch alignment)

3. **Pruning Strategy**:
   - Sorts segments by match candidate density (ranking)
   - Uses progressive RMSD cutoff (tighter as more segments match)
   - Prunes based on distance deviations between segments

### Key Functions

From `Search.cpp` (1,647 lines):
- `searchByDistDistr()`: Main entry point for search
- `auxSearchByDistDistrBB()`: Backbone RMSD search (core algorithm)
- `auxSearchByDistDistrCA()`: CA-only RMSD search
- `calcRmsdKabsch()`: RMSD calculation using Kabsch algorithm

## Main Data Structures

### Core Hierarchy (msttypes.h)

**Structure → Chain → Residue → Atom**:
```
Structure
  ├── vector<Chain*> chains
  ├── map<string, Chain*> chainsByID
  └── map<string, Chain*> chainsBySegID

Chain
  ├── vector<Residue*> residues
  ├── map<Residue*, int> residueIndexInChain
  └── Structure* parent

Residue
  ├── vector<Atom*> atoms
  ├── string resname
  ├── int resnum
  └── Chain* parent

Atom
  ├── CartesianPoint coords (vector<double>)
  ├── string name
  └── Residue* parent
```

### Protein Structure Classes

**ProteinStruct** (base class):
- `vector<vector<double>> _bbcoor`: Backbone coordinates (per residue)
- `vector<vector<double>> _cacoor`: CA coordinates
- `vector<bool> _fullbb`: Full backbone flag per residue
- `map<pair<int, int>, double> _dist`: Distance cache
- `Structure _sys`: Full structure representation
- `FILE* _ifp`: File pointer for binary PDS format

**QueryStruct** (inherits ProteinStruct):
- Represents the query structure
- Methods for reading query PDS files
- Coordinate extraction for RMSD calculation

**TargetStruct** (inherits ProteinStruct):
- Represents database structures
- **Distance distributions**: `vector<vector<vector<int>>> _distdistr`
  - Pre-computed distribution of distances between residue pairs
  - Binned by distance (dcut, dstep parameters)
- **Dihedral distributions**: `vector<vector<vector<int>>> _diheddistr`
  - Pre-computed phi/psi angle distributions
  - Binned by angle (phistep, psistep parameters)
- Methods for reading target PDS files
- Fast lookup methods: `readDistDistr()`, `readDihedDistr()`

### Search Data Structures

**SearchResults**:
- `vector<Match*> matchSet`: Found matches
- `MatchLeast`, `MatchMost`: Min/max matches to return
- `QueryStruct qs`: Query structure
- `vector<string> tsFiles`: Target structure file paths

**Match**:
- Stores mapping between query and target residues
- Segment-to-segment correspondences
- RMSD value

**CenResGrp** (Center Residue Groups):
- Groups query segments by center residues
- **Ranking**: Match candidate density (candidates/segment_length)
- **Pruning**: Sorts by rank, processes most promising first
- **State tracking**: Current match candidate index, flags (MATCH_L1, MATCH_L2, NOT_MATCH)

### Distribution Data Structures

**Distance Distribution** (`_distdistr`):
- `vector<vector<vector<int>>>`: [res_i][res_j][distance_bin] → list of target residues
- Enables O(1) lookup: given query residues i,j and distance, get candidate target residues

**Dihedral Distribution** (`_diheddistr`):
- `vector<vector<vector<int>>>`: [res_i][phi_bin][psi_bin] → list of target residues
- Pre-filters by backbone dihedral angles

### Memory Characteristics

**Pre-computed Data** (TargetStruct):
- Distance distributions: O(N² × bins) where N = residues, bins = distance bins
- Dihedral distributions: O(N × phi_bins × psi_bins)
- **Trade-off**: Large memory footprint for fast search

**Runtime Data**:
- Query structure: Small (single structure in memory)
- Search state: Moderate (CenResGrp, match candidates)
- Matches: Small (typically <100 matches per query)

## Integration with OSPREY

### Current Usage (MONTAGE.py)

```python
# 1. Convert PDB to PDS format
subprocess.run(["./resources/createPDS", "--type", "query", 
                "--pdb", query.pdb, "--pds", "query.pds"])

# 2. Search database
subprocess.run(["./resources/master", "--query", "query.pds",
                "--targetList", "db.txt.local", "--rmsdCut", "10.0",
                "--topN", "10", "--outType", "match",
                "--seqOut", "matches.txt", "--structOut", "matches"])
```

**Overhead**:
- Subprocess creation: ~100-500ms per call
- File I/O: PDS file read/write
- Process termination: ~10-50ms

**Total overhead**: ~200-600ms per MASTER call (negligible compared to 12s execution time)

### Optimization Opportunities

**Library Integration** (vs. subprocess):
- **Benefit**: Eliminate subprocess overhead (~200-600ms saved)
- **Benefit**: In-memory data passing (no PDS file I/O)
- **Benefit**: Shared memory for database (no repeated file reads)
- **Impact**: Small (~2-5% improvement) but cleaner architecture

**Parallelization**:
- Current: Sequential database search
- Opportunity: Parallel target structure searches (independent targets)
- **Benefit**: Near-linear speedup with multiple threads/processes
- **Impact**: Could reduce 12s to ~2-3s on 4-8 cores

**Database Caching**:
- Current: Reads target structures from disk for each query
- Opportunity: Cache target structures in memory (if memory allows)
- **Benefit**: Eliminate disk I/O for repeated searches
- **Impact**: Depends on database size and available memory

## Code Quality Assessment

### Strengths

1. **Clear architecture**: Well-defined class hierarchy
2. **Efficient algorithm**: Multi-level pruning reduces search space
3. **Binary format**: PDS format enables fast lookups
4. **Modular design**: Separate classes for query, target, search, results

### Weaknesses

1. **No parallelism**: Sequential search through database
2. **File-based I/O**: No in-memory database option
3. **Legacy C++**: Uses older C++ style (pre-C++11)
4. **No SIMD**: Distance calculations could benefit from vectorization

### Modernization Potential

**C++11/14/17 features**:
- Smart pointers (unique_ptr, shared_ptr) for memory management
- Range-based for loops
- Auto keyword
- Lambda functions for callbacks
- std::optional for nullable values

**Performance improvements**:
- SIMD vectorization for distance calculations
- Parallel database search (std::thread, OpenMP)
- Memory-mapped files for database access

## Recommendations

### For OSPREY Pipeline

1. **Status**: MASTER performance is adequate (2.9% of time)
2. **Priority**: Low - focus optimization on ConfSpace compilation (80% of MONTAGE time) and K* (1-3 hours)
3. **Integration**: Library integration (vs. subprocess) is architectural improvement, not performance critical

### For Unified C++ Pipeline

1. **Library Integration**: 
   - Extract MASTER core as library (not CLI)
   - In-memory API: `search(query_structure, target_database) -> matches`
   - Eliminate PDS file I/O, subprocess overhead

2. **Parallelization**:
   - Parallel target searches (independent)
   - Thread pool or MPI ranks for database search
   - Shared memory for target structures

3. **Memory Management**:
   - Memory-mapped files for large databases
   - Cache frequently-used target structures

## References

- **Paper**: Zhou, Grigoryan (2015), "Rapid search for tertiary fragments reveals protein sequence–structure relationships". Protein Science, 24: 508–524. doi: 10.1002/pro.2610
- **Website**: http://www.grigoryanlab.org/master/
- **Source**: `master-v1.6/` directory (GNU LGPL v3)
- **OSPREY Usage**: `src/main/python/CCKStar/MONTAGE.py::submit_MASTER()`

