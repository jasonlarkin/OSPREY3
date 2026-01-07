# Ecosystem references: energy engines and conformation/search packages

References (developer docs + code entry points):

- molecular system representation (topology, parameters, coordinates, constraints)
- potential/force term composition and extensibility
- energy/force evaluation pipelines (including GPU kernels)
- conformation search / sampling operators and objective functions
- integration surfaces (C++ APIs, plugin systems, Python bindings)

This is a reading list, not an endorsement of any specific architecture.

## Molecular simulation / energy engines (classical MD + hybrid)

### LAMMPS

Seems not to have changed much since I last used it:

- **Project**: [LAMMPS](https://www.lammps.org/)
- **Code**: [lammps/lammps](https://github.com/lammps/lammps)
- **Manual**: [LAMMPS documentation](https://docs.lammps.org/)

Interfaces:

- **Style system (plugin-by-registration)**:
  - `src/pair_*.cpp` / `src/pair_*.h` (pair potentials)
  - `src/bond_*.cpp`, `src/angle_*.cpp`, `src/dihedral_*.cpp`, `src/improper_*.cpp`
  - `src/fix_*.cpp` (time integration, constraints, thermostats, etc.)
  - `src/compute_*.cpp` (observables)
- **Key pattern**: family of **Style** base classes + factory/registration macros + runtime selection via input script keywords.

### GROMACS

- **Project**: [GROMACS](https://www.gromacs.org/)
- **Code**: [gromacs/gromacs](https://github.com/gromacs/gromacs)
- **Manual**: [GROMACS documentation](https://manual.gromacs.org/documentation/current/)
- **Workflow API**: [gmxapi](https://manual.gromacs.org/current/gmxapi/userguide/overview.html)

Interfaces:

- **Force computation pipeline**: search for "nonbonded kernels / nbnxm" in `src/gromacs/`
- **Key pattern**: performance-critical kernels are organized separately from higher-level simulation control; stable external workflow API via `gmxapi`.

### OpenMM

- **Project**: [OpenMM](https://openmm.org/)
- **Code**: [openmm/openmm](https://github.com/openmm/openmm)
- **Docs**: [OpenMM docs](https://docs.openmm.org/latest/)

Interface:

- **Composable energy terms**: `Force` objects attached to a `System`
  - built-ins + "user-defined" via `Custom*Force` classes
- **Execution backend abstraction**: `Platform` + kernel implementations 
- **Key pattern**: clean separation between energy term (front-end object graph) and platform-specific kernels.

### HOOMD-blue

- **Project**: [HOOMD-blue](https://hoomd-blue.readthedocs.io/)
- **Code**: [glotzerlab/hoomd-blue](https://github.com/glotzerlab/hoomd-blue)

Interface:

- Python-first API mapping to C++
- Pair potentials and integrators structured as interchangeable components

### espressomd

- **Project**: [ESPResSo](https://espressomd.org/)
- **Code**: [espressomd/espresso](https://github.com/espressomd/espresso)

### TINKER / TINKER-HP

- **TINKER**: [TINKER](https://dasher.wustl.edu/tinker/)
- **TINKER-HP code**: [TinkerTools/tinker-hp](https://github.com/TinkerTools/tinker-hp)

Polarizable force fields (AMOEBA-family) and HPC decomposition strategies.

### NAMD / VMD ecosystem

- **NAMD**: [NAMD](https://www.ks.uiuc.edu/Research/namd/)
- **User guide**: [NAMD user guide](https://www.ks.uiuc.edu/Research/namd/2.14/ug/)
- **VMD**: [VMD](https://www.ks.uiuc.edu/Research/vmd/)

- scalable MD control plane
- integration surfaces (Tcl scripting, colvars, plugins)

Local tooling:

- `interfaces/tools/fetch_namd_doxygen_api.py` parses NAMD’s Doxygen class hierarchy into JSON/Markdown.

### AMBER / AmberTools

- **AMBER**: [AMBER](https://ambermd.org/)
- **Manuals**: [AMBER manuals](https://ambermd.org/Manuals.php)

- topology/parameter representation
- classical FF pipelines + common biomolecular workflows

### CHARMM

- **CHARMM**: [CHARMM](https://www.charmm.org/)
- **Docs**: [CHARMM documentation](https://www.charmm.org/documentation/)

### PLUMED (enhanced sampling plugin)

- **PLUMED**: [PLUMED](https://www.plumed.org/)
- **Code**: [plumed/plumed2](https://github.com/plumed/plumed2)


- plugin model (hooking collective variables and biases into multiple MD engines)

## Quantum chemistry / electronic structure (QM backends)

Electron structure as a backend energy term:

- **Psi4**: [Psi4](https://psicode.org/), code: [psi4/psi4](https://github.com/psi4/psi4)
- **PySCF**: [PySCF](https://pyscf.org/), code: [pyscf/pyscf](https://github.com/pyscf/pyscf)
- **NWChem**: [NWChem](https://nwchemgit.github.io/), code: [nwchemgit/nwchem](https://github.com/nwchemgit/nwchem)
- **CP2K**: [CP2K](https://www.cp2k.org/), code: [cp2k/cp2k](https://github.com/cp2k/cp2k)
- **Quantum ESPRESSO**: [Quantum ESPRESSO](https://www.quantum-espresso.org/)

- treat QM as an `IPotentialTerm`/`IEnergyEvaluator` capability with explicit resource limits and caching (basis sets, grids, SCF reuse).

## Conformation search / protein design / docking

### OSPREY

- **Project**: [OSPREY](https://www.cs.duke.edu/donaldlab/osprey.php)
- **Code**: [donaldlab/OSPREY3](https://github.com/donaldlab/OSPREY3)


- discrete conformation spaces (rotamers, RCs)
- pruning + best-first search patterns
- bounds/heuristics and energy decomposition assumptions

### Rosetta

- **Project**: [RosettaCommons](https://www.rosettacommons.org/)
- **Docs**: [Rosetta docs](https://www.rosettacommons.org/docs/latest/Home)
- **Code**: [RosettaCommons/rosetta](https://github.com/RosettaCommons/rosetta)

Interface:

- **Operators**: **Mover** abstraction (state-transforming search/sampling step)
- **Objective**: **ScoreFunction** abstraction (weighted energy terms)
- **Key pattern**: a pipeline of modular moves + scoring + acceptance criteria; extensive plugin-like extension via compiled components and XML protocol wiring.

### AutoDock Vina (docking / local search)

- **Project**: [AutoDock Vina](http://vina.scripps.edu/)
- **Code**: [ccsb-scripps/AutoDock-Vina](https://github.com/ccsb-scripps/AutoDock-Vina)


- rigid-body + torsion state representation
- scoring function structure
- iterated local search patterns

### GNINA / Smina (docking variants)

- **GNINA**: code: [gnina/gnina](https://github.com/gnina/gnina)
- **Smina**: code: [mfursov/smina](https://github.com/mfursov/smina)

### DOCK6

- **Project**: [DOCK6](https://dock.compbio.ucsf.edu/DOCK_6/)

### rDock

- **Project**: [rDock](http://rdock.sourceforge.net/)
- **Code**: [rDock/rDock](https://github.com/rDock/rDock)

## Small-molecule conformer generation / minimization toolkits

### RDKit

- **Project**: [RDKit](https://www.rdkit.org/)
- **Code**: [rdkit/rdkit](https://github.com/rdkit/rdkit)
- **Docs**: [RDKit docs](https://www.rdkit.org/docs/)

Relevant API:

- distance geometry + ETKDG conformer generation (multiple conformers)
- MMFF/UFF minimization and force-field wrappers

### Open Babel

- **Project**: [Open Babel](https://openbabel.org/)
- **Code**: [openbabel/openbabel](https://github.com/openbabel/openbabel)

## Local doc tooling (HTML/PDF)

- `interfaces/tools/fetch_web_doc.py`: fetch a doc page, cache `page.html`, extract `page.txt`, and emit `links.json` + `summary.md`
- `interfaces/tools/convert_pdf.py`: convert PDFs to greppable text and/or page images (prefers poppler tools)

## Algorithm families to support (mapping to interfaces)

What abstract interface must expose.

- **Best-first / A\***: needs admissible `LB(state)` or heuristic + consistent cost semantics
- **Branch-and-bound**: needs `LB(partial_state)` and tight incremental bounds
- **Beam search**: needs neighborhood generation + ranking; no correctness guarantees
- **MCTS / stochastic search**: needs rollout policy hooks; benefits from cheap $\Delta$`E`
- **Local search (SA / tabu)**: needs move operators and fast evaluation; often continuous refinement steps
- **Hybrid discrete-continuous**: needs **discrete assignment** + **continuous relax/minimize** as first-class moves
- **MILP/ILP/QP**: needs a representation extractable to linear/quadratic forms (where possible)

## Integration pattern checklist

- **State representation**: explicit DOFs vs Cartesian; copy cost; hashing/equality; partial states
- **Move operators**: locality metadata; deterministic ordering; composability
- **Energy term API**: total energy, delta energy, gradients, Hessians (optional), bounds (optional)
- **Execution context**: thread-safety, caching, scratch buffers, GPU/CPU dispatch
- **Determinism**: tie-breaking, FP reproducibility, stable iteration orders
- **Proof/diagnostics**: incumbent curves, bounds, traces, explainability hooks

