# Interfaces: Generalizing OSPREY’s Problem

This directory collects interface sketches and ecosystem references for generalizing OSPREY-style computation:

- energy modeling for molecular systems (classical + hybrid + QM backends)
- search/optimization over conformations (discrete, continuous, hybrid)

The goal is to separate **physics** (energy) from **state space** (conformations/moves) from **search** (algorithms), so multiple search algorithms can be swapped in without rewriting the domain model.

See `PHYSICS_STATE.md` for analysis of how this separation maps to different package designs (classical MD, quantum mechanics, protein design).

## Reference packages

- See `interfaces/ECOSYSTEM_REFERENCES.md` for curated technical references (docs + code entry points).
- See `interfaces/ANALYSIS_PLAN.md` for the normalized rubric and the command sequence.
- See `interfaces/DOC_INDEX.md` for doc entry points discovered in cloned repos.

## Core abstraction split

- **State space**: conformation degrees of freedom (DOFs), constraints, move set, goal predicate
- **Physics model**: energy terms and how they compose, including bounds and deltas
- **Search engine**: algorithm over an abstract problem interface, independent of chemistry details
- **Bridging layer**: adapters that map OSPREY’s current rotamer/emat/A* design into the abstract interfaces

## Minimal interfaces for a general “energy over molecular systems” problem

### System and conformation

- **`IMolecularSystem`**
  - atoms, topology, parameters
  - rigid groups, symmetry, reference frames
  - supports “views” (subsets) for local energy evaluation
- **`IConformation`**
  - lightweight value/handle describing DOF assignments
  - not required to be Cartesian coordinates
- **`IKinematics`**
  - realizes Cartesian coordinates from DOFs
  - supports partial realization for local moves

Separation constraint: DOFs are a compact state; geometry realization is a derived view computed lazily as needed by energy terms.

### Degrees of freedom and moves

- **`IDofSpace`**
  - enumerates DOFs and their domains:
    - discrete rotamers / discrete grids
    - continuous torsions
    - rigid-body transforms (position/orientation)
    - hybrid mixtures
- **`IMove`**
  - an edit to a state (small value type)
  - examples: change rotamer at position, perturb torsion, rigid-body delta, local minimization step
- **`INeighborhood`**
  - produces moves from a state, optionally ordered

Separation constraint: neighborhood generation belongs to the state space layer, not the search algorithm and not the energy model.

### Energy and potentials

Energy should be modeled as a sum of terms with explicit capabilities. Do not force pairwise decomposition.

- **`IPotentialTerm`**
  - computes contribution on realized geometry
  - optionally supports partial evaluation (locality) and/or bounds
- **`IEnergyModel`**
  - owns term list, weights, coupling rules, units, precision policy

Expose these capabilities (optional, not mandatory):

- **Total energy**: `E(state)` (correctness baseline)
- **Incremental energy**: `ΔE(state, move)` when the model supports fast deltas
- **Lower bounds**: `LB(partial_state)` for pruning (branch-and-bound) and admissible A* heuristics
- **Locality metadata**: which atoms/residues a move touches, enabling partial evaluation

### Evaluation context

- **`IEnergyEvaluator`**
  - thread-safe evaluator with explicit caches and scratch memory
  - backend selection (classical FF vs hybrid vs QM)
- **`EvalContext`**
  - precision mode, cutoff policy, determinism controls, resource limits

This is the seam for: classical force fields, implicit solvent, knowledge-based potentials, GPU kernels, and QM backends.

## Search problem interface

Define the search/optimization contract independently of chemistry and independently of “pairwise matrix” assumptions.

- **`ISearchProblem`**
  - state type
  - neighborhood generator
  - goal predicate (or “enumerate best-K” objective)
  - objective/cost function derived from energy + penalties/constraints
  - optional heuristic and bounds
- **`ISearchAlgorithm`**
  - consumes `ISearchProblem`
  - returns solutions + diagnostics (incumbent curve, bound gap, proofs where applicable)

Proof artifacts (when the algorithm provides guarantees):

- optimality certificate / bound gap
- incumbent over time (anytime behavior)
- reproducibility controls (tie-breaking, ordering, determinism mode)

## Concrete C++ shape that stays fast

Use type-erasure for integration boundaries; use templates for hot loops.

```cpp
// Concept sketch only. For hot paths: make State a concrete value type and template the evaluator/neighborhood.

struct Move { /* small value type */ };

class IState {
public:
  virtual ~IState() = default;
  virtual std::unique_ptr<IState> clone() const = 0;
};

class INeighborhood {
public:
  virtual ~INeighborhood() = default;
  virtual void enumerate(const IState& s, std::vector<Move>& out) const = 0;
};

class IEnergyEvaluator {
public:
  virtual ~IEnergyEvaluator() = default;
  virtual double energy(const IState& s) const = 0;
  virtual bool supports_delta() const = 0;
  virtual double delta_energy(const IState& s, const Move& m) const = 0;
  virtual bool supports_lower_bound() const = 0;
  virtual double lower_bound(const IState& partial) const = 0;
};

class ISearchProblem {
public:
  virtual ~ISearchProblem() = default;
  virtual const INeighborhood& neighborhood() const = 0;
  virtual const IEnergyEvaluator& evaluator() const = 0;
  virtual bool is_goal(const IState& s) const = 0;
};

struct SearchResult {
  std::unique_ptr<IState> best;
  double best_energy = 0.0;
  // traces, bounds, diagnostics
};

class ISearchAlgorithm {
public:
  virtual ~ISearchAlgorithm() = default;
  virtual SearchResult run(const ISearchProblem& problem) = 0;
};
```

## How this generalizes OSPREY’s current design

- OSPREY’s **positions and rotamers** map to `IDofSpace` with discrete domains.
- OSPREY’s **energy matrix** is one `IEnergyEvaluator` backend supporting `ΔE` and strong lower bounds.
- A* becomes one `ISearchAlgorithm`; the same problem can be run with branch-and-bound, beam search, anytime variants, MCTS, local search, or hybrid discrete-continuous methods.

## Non-negotiable separations (to preserve extensibility)

- DOF mapping and kinematics must not be embedded in the energy evaluator.
- Energy term implementations must not assume pairwise decomposability.
- Search algorithms depend only on neighborhood + objective/bounds, not on chemistry types.
- Bounds and deltas are optional capabilities, not required methods.

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
- **SCWRL4** (side-chain prediction via rotamers; classic baseline): [SCWRL4](http://dunbrack.fccc.edu/scwrl4/)
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

