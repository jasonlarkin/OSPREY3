# Generalizing Interfaces

This directory collects interface sketches and ecosystem references for generalizing:

- energy modeling for molecular systems (classical + hybrid + QM backends)
- search/optimization over conformations (discrete, continuous, hybrid)

## Reference packages

- See `interfaces/ECOSYSTEM_REFERENCES.md` for curated technical references (docs + code entry points).
- See `interfaces/DOC_INDEX.md` for doc entry points discovered in cloned repos.

## Requirements

- CMake 3.16 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- No external dependencies required (interfaces and adapters are header-only or use standard library only)

## Building

The interfaces and adapter stubs can be built using CMake:

```bash
cd interfaces
cmake -B build -S .
cmake --build build
```

This builds:
- `libnext-osprey-adapters.a` - OSPREY adapter stubs
- `libnext-rosetta-adapters.a` - Rosetta adapter stubs
- `libnext-openmm-adapters.a` - OpenMM adapter stubs
- `libnext-openbabel-adapters.a` - Open Babel adapter stubs
- `interface-demo-all` - Demo executable showcasing all adapters

Run the demo:
```bash
./build/interface-demo-all
```

### Demo Output

```
=== Generalized Molecular Energy and Search Interfaces Demo ===

1. OSPREY Adapters (Rotamer-based Search)
   - EnergyMatrixTerm: precomputed pairwise energy matrix
   - RotamerConformation: discrete rotamer assignments
   - Supports delta_energy: 1
   - Supports lower_bound: 1

2. Rosetta Adapters (Protein Design & Scoring)
   - ScoreFunctionEvaluator: weighted composition of score terms
   - PoseState: molecular structure state container
   - Supports delta_energy: 0

3. OpenMM Adapters (Molecular Dynamics)
   - ForceTerm: individual force field terms (bonds, angles, etc.)
   - SystemWrapper: topology and particle system
   - ContextState: runtime state with positions/velocities
   - ForceTerm supports delta_energy: 0
   - System has 0 atoms

4. Open Babel Adapters (Conformer Generation)
   - OBForceFieldTerm: molecular mechanics force field
   - Supports delta_energy: 0

All adapters successfully instantiated!
Interface demonstrates:
  - Unified abstraction across different molecular simulation packages
  - Package-specific adapters maintain their native capabilities
  - Search algorithms can operate on any conforming adapter
```

The demo demonstrates:
- OSPREY adapters supporting incremental evaluation (delta_energy, lower_bound)
- Rosetta/OpenMM/Open Babel adapters providing basic evaluation
- Polymorphic usage through the `next` interfaces

See `interfaces/INTERFACE_DESIGN.md` for detailed interface documentation.

## Core abstraction split

- **State space**: conformation degrees of freedom (DOFs), constraints, move set, goal predicate
- **Physics model**: energy terms and how they compose, including bounds and deltas
- **Search engine**: algorithm over an abstract problem interface, independent of chemistry details
- **Bridging layer**: adapters that map OSPREY’s current rotamer/emat/A* design into the abstract interfaces (**NOTE**: continuing to use this as concrete motivation, but expanding beyond via the list of other packages)

# README Update Proposal: Reconciling Design and Implementation

## Current Discrepancy

### README.md Describes (Conceptual Design)
- `IMolecularSystem` - atoms, topology, parameters, rigid groups, symmetry, reference frames, "views"
- `IKinematics` - realizes Cartesian coordinates from DOFs, supports partial realization
- `IDofSpace` - enumerates DOFs and their domains (discrete, continuous, hybrid)
- `IPotentialTerm` / `IEnergyModel` - energy modeling
- `EvalContext` - precision mode, cutoff policy, determinism controls
- Statement: "search/optimization contract independently of chemistry"

### Actually Implemented (`include/next/`)
- `ISystem` - topology and composition (simpler, focused on what's needed)
- `IState` - runtime state with coordinates (simpler, no kinematics abstraction)
- `IConformation` - compact conformation representation (exists)
- `IEnergyTerm` / `IEnergyEvaluator` - energy evaluation (exists)
- `IMove`, `INeighborhood`, `ISearchProblem`, `ISearchAlgorithm` - search components (all exist)
- No `IDofSpace`, `IKinematics`, `EvalContext` abstractions

## Analysis

### What Was Simplified

1. **`IMolecularSystem` → `ISystem`**: 
   - Removed: rigid groups, symmetry, reference frames, "views" (subsets)
   - Kept: topology, atoms, energy terms
   - **Rationale**: These features can be added later if needed. Core interface focuses on essential separation (topology vs state).

2. **`IKinematics` → Not implemented**:
   - Removed: explicit DOF → Cartesian coordinate conversion abstraction
   - **Rationale**: This conversion can be handled within adapters or `IConformation::to_state()`. Not all conformations need DOF representation (some are already Cartesian).

3. **`IDofSpace` → Not implemented**:
   - Removed: explicit enumeration of DOF domains
   - **Rationale**: DOF domains are implicit in `INeighborhood::generate_moves()`. The neighborhood generator knows what moves are valid without needing an explicit DOF space object.

4. **`EvalContext` → Not implemented**:
   - Removed: precision mode, cutoff policy, determinism controls
   - **Rationale**: These are configuration details that can be passed to evaluators via constructors/options. Not part of the core interface contract.

### What Should Be Clarified

1. **"Search/optimization independently of chemistry"**:
   - **Current**: Too vague
   - **Clarification needed**: Search is primarily over **energy** (as computed by physics models), with possible **constraints** and **multi-objective optimization** scenarios.

2. **Search problem definition**:
   - `ISearchProblem::cost()` uses `IEnergyEvaluator::evaluate()`
   - But search could involve multiple objectives, constraints, penalties
   - Need to clarify: energy is the primary objective, but not the only one

## Proposed Updates

### Section: "Search problem interface"

**Proposed replacement**:
```
Define the search/optimization contract that abstracts over specific energy models:

Search algorithms operate on:
- **Primary objective**: Energy (computed by physics models via `IEnergyEvaluator`)
- **Constraints**: Hard constraints (e.g., bond lengths, angles) or soft constraints (penalties)
- **Multi-objective scenarios**: Multiple energy terms or objectives (e.g., energy + penalty functions)

The search contract is "chemistry-aware" in that it operates on molecular energy, but "chemistry-agnostic" in that it doesn't depend on specific force fields, quantum methods, or molecular representations. The same search algorithm can operate on classical force fields, quantum mechanical energies, or hybrid models.
```

### Section: "System and conformation"

Separation constraint: DOFs are a compact state; geometry realization is a derived view computed lazily as needed by energy terms.
```

**Proposed replacement**:
```
**Implemented interfaces** (see `include/next/`):

- **`ISystem`**
  - Molecular topology and composition (atoms, bonds, constraints)
  - Owns list of energy terms
  - Pattern: Separates immutable topology from mutable runtime state
  
- **`IState`**
  - Runtime molecular state (coordinates, velocities, cached data)
  - Binds to an `ISystem`
  - Provides coordinate access: `set_coordinates()`, `get_coordinates()`
  
- **`IConformation`**
  - Compact conformation representation (DOF assignments)
  - Not necessarily Cartesian coordinates - can be discrete (rotamer indices) or continuous (angles, distances)
  - Converts to/from `IState` via `to_state()` and `from_state()`

**Design notes**:
- DOF → Cartesian conversion is handled by `IConformation::to_state()` rather than a separate `IKinematics` interface
- Rigid groups, symmetry, reference frames are not in the core interface but can be handled by specific `ISystem` implementations
- Local energy evaluation ("views") is supported via `IMove::affected_atoms()` and incremental evaluation (`delta_energy()`)
```

### Section: "Degrees of freedom and moves"

Separation constraint: neighborhood generation belongs to the state space layer, not the search algorithm and not the energy model.
```

**Proposed replacement**:
```
**Implemented interfaces**:

- **`IMove`**
  - Move descriptor (state mutation) with locality metadata
  - Small value type describing a state change
  - Examples: change rotamer at position, perturb torsion, rigid-body delta, local minimization step
  - Provides `affected_atoms()` and `affected_residues()` for incremental evaluation

- **`INeighborhood`**
  - Generates moves from a conformation
  - Optional: ordered/ranked moves (for best-first search)
  - DOF domains are implicit: `generate_moves()` produces valid moves for the conformation type

**Design notes**:
- DOF space enumeration (`IDofSpace`) is not needed - the neighborhood generator knows valid moves without explicit DOF enumeration
- Neighborhood generation belongs to the state space layer, not the search algorithm or energy model
- Move types vary by conformation representation: discrete (rotamer changes), continuous (coordinate perturbations), hybrid
```

### Section: "Energy and potentials"

**Proposed replacement**:
```
**Implemented interfaces**:

- **`IEnergyTerm`**
  - Single energy contribution (composable building block)
  - Main evaluation: `evaluate(const IState& state)`
  - Optional capabilities (declared, not required):
    - `delta_energy()` - incremental evaluation for efficiency
    - `lower_bound()` - lower bound for partial states (branch-and-bound)
    - `supports_gradients()` - gradient computation

- **`IEnergyEvaluator`**
  - Composed energy evaluator (sum of multiple terms)
  - Delegates to terms and checks capability consistency
  - Supports same optional capabilities if all terms support them

**Design notes**:
- Energy terms are composable: add/remove terms to build complex energy models
- Capabilities are optional: algorithms check `supports_delta_energy()` before using incremental evaluation
- Units, precision, cutoff policies are handled by term implementations, not the core interface
```

## Action Items

1. Update README.md to match implemented interfaces
2. Clarify "abstracts over energy models but operates on molecular energy"
3. Remove references to unimplemented interfaces (`IMolecularSystem`, `IKinematics`, `IDofSpace`, `EvalContext`)
5. Potential future extensions (rigid groups, symmetry, etc.) as "future enhancements" rather than core design



---

## Ecosystem references: molecular simulation / energy engines

These are widely used packages relevant to “atomic energy of molecular systems” and multi-term potentials.

- **LAMMPS** (MD, flexible force-fields, many-body terms, plugins): [LAMMPS](https://www.lammps.org/), code: [lammps/lammps](https://github.com/lammps/lammps)
- **GROMACS** (high-performance biomolecular MD): [GROMACS](https://www.gromacs.org/), code: [gromacs/gromacs](https://github.com/gromacs/gromacs)
- **AMBER** (MD engine + force fields; AmberTools): [AMBER](https://ambermd.org/)
- **NAMD** (scalable biomolecular MD): [NAMD](https://www.ks.uiuc.edu/Research/namd/)
- **OpenMM** (GPU-centric MD toolkit/library): [OpenMM](https://openmm.org/), code: [openmm/openmm](https://github.com/openmm/openmm)
- **CHARMM** (MD + force fields): [CHARMM](https://www.charmm.org/)
- **TINKER / TINKER-HP** (force fields, polarizable models): [TINKER](https://dasher.wustl.edu/tinker/), [TINKER-HP](https://github.com/TinkerTools/tinker-hp)
- **NWChem** (QM + MD-related capabilities): [NWChem](https://nwchemgit.github.io/), code: [nwchemgit/nwchem](https://github.com/nwchemgit/nwchem)
- **Psi4** (QM): [Psi4](https://psicode.org/), code: [psi4/psi4](https://github.com/psi4/psi4)
- **PySCF** (QM): [PySCF](https://pyscf.org/), code: [pyscf/pyscf](https://github.com/pyscf/pyscf)
- **Quantum ESPRESSO** (DFT): [Quantum ESPRESSO](https://www.quantum-espresso.org/)
- **CP2K** (DFT/MD, mixed Gaussian/plane-wave): [CP2K](https://www.cp2k.org/)
- **ORCA** (QM; not open source): [ORCA](https://orcaforum.kofo.mpg.de/)

Force-field / analysis / interoperability tooling (adjacent but relevant):

- **MDAnalysis** (trajectory analysis): [MDAnalysis](https://www.mdanalysis.org/), code: [MDAnalysis/mdanalysis](https://github.com/MDAnalysis/mdanalysis)
- **ParmEd** (parameter/topology manipulation): [ParmEd](https://parmed.github.io/ParmEd/), code: [ParmEd/ParmEd](https://github.com/ParmEd/ParmEd)
- **RDKit** (cheminformatics; conformers): [RDKit](https://www.rdkit.org/), code: [rdkit/rdkit](https://github.com/rdkit/rdkit)

---

## Ecosystem references: conformation search / design / optimization

These are relevant to “search over conformations” in protein/ligand contexts, plus general molecular conformer generation.

Protein design / rotameric and hybrid search:

- **OSPREY** (protein design, K*, DEE/A*, etc.): [OSPREY](https://www.cs.duke.edu/donaldlab/osprey.php), code: [donaldlab/OSPREY3](https://github.com/donaldlab/OSPREY3)
- **Rosetta** (protein modeling/design; many search components): [RosettaCommons](https://www.rosettacommons.org/), code: [RosettaCommons/rosetta](https://github.com/RosettaCommons/rosetta)
- **SCWRL4** (side-chain prediction via rotamers; classic baseline): [SCWRL4](https://dunbrack.fccc.edu/lab/scwrl)
- **FoldX** (protein stability/design scoring; not fully open): [FoldX](https://foldxsuite.crg.eu/)

General molecular conformer generation / sampling:

- **RDKit** (ETKDG conformers + minimization): [RDKit](https://www.rdkit.org/), code: [rdkit/rdkit](https://github.com/rdkit/rdkit)
- **Open Babel** (conformer generation + chemistry utilities): [Open Babel](https://openbabel.org/), code: [openbabel/openbabel](https://github.com/openbabel/openbabel)

Docking / pose search (search over ligand rigid-body + torsions):

- **AutoDock Vina**: [AutoDock Vina](http://vina.scripps.edu/), code: [ccsb-scripps/AutoDock-Vina](https://github.com/ccsb-scripps/AutoDock-Vina)
- **GNINA** (Vina-like + ML scoring): [gnina](https://github.com/gnina/gnina)

Specialized sampling / enhanced sampling (often MD-driven rather than combinatorial search):

- **PLUMED** (enhanced sampling plugin): [PLUMED](https://www.plumed.org/), code: [plumed/plumed2](https://github.com/plumed/plumed2)

Algorithmic families to support via `ISearchAlgorithm` (targets, not packages):

- A* variants (admissible heuristics, anytime A*)
- branch-and-bound / best-first with bounds
- beam search
- MCTS / stochastic search
- local search (hill climbing, simulated annealing, tabu)
- ILP / MILP (where energy can be expressed as linear/quadratic forms)
- hybrid discrete-continuous: discrete assignment + local minimization / continuous refinement

