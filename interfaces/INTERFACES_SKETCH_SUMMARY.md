# Interfaces Sketch: Navigation Guide

## Overview

Unified C++ abstraction layer for molecular energy evaluation and conformation search across simulation packages (OpenMM, Rosetta, OSPREY, Open Babel, LAMMPS, GROMACS, HOOMD-blue, QM packages). Designed for fixed-topology simulations (conformation search, standard MD, protein design). Separates physics models (energy computation) from state representations (coordinates/conformations). **Scope**: Fixed-topology use cases; reactive MD (bond breaking/forming) not supported.

## Core Design Documents

- [INTERFACE_DESIGN.md](INTERFACE_DESIGN.md) - Design principles, package adaptations, capability matrix
- [INTERFACE_DIAGRAM.md](INTERFACE_DIAGRAM.md) - Class hierarchy diagram
- [ECOSYSTEM_REFERENCES.md](ECOSYSTEM_REFERENCES.md) - Package API references and integration patterns
- [COMPARISON_TABLE.md](COMPARISON_TABLE.md) - Interface comparison across packages

## Core Interfaces (9 headers)

Location: [`include/next/`](include/next/)

- `ISystem.h` - Molecular topology and composition
- `IState.h` - Runtime molecular state (coordinates, velocities)
- `IEnergyTerm.h` - Single energy contribution (composable)
- `IEnergyEvaluator.h` - Composed energy evaluator (sum of terms)
- `IConformation.h` - Compact conformation representation (DOF assignments)
- `IMove.h` - Move descriptor (state mutation with locality metadata)
- `INeighborhood.h` - Neighborhood generator (moves from a state)
- `ISearchProblem.h` - Search problem definition
- `ISearchAlgorithm.h` - Abstract search algorithm

## Package Adapters (stubs)

Location: [`adapters/`](adapters/)

### OSPREY (discrete rotamer search)
- `adapters/osprey/EnergyMatrixTerm.h` - Precomputed pairwise energy matrix
- `adapters/osprey/RotamerConformation.h` - Discrete rotamer assignments
- `adapters/osprey/RotamerMove.h` - Rotamer change descriptors
- `adapters/osprey/RotamerNeighborhood.h` - Rotamer neighborhood generation

### Rosetta (protein design)
- `adapters/rosetta/ScoreFunctionEvaluator.h` - Weighted score term composition
- `adapters/rosetta/PoseState.h` - Pose state wrapper
- `adapters/rosetta/MoverNeighborhood.h` - Mover-based neighborhood generation

### OpenMM (molecular dynamics)
- `adapters/openmm/SystemWrapper.h` - System topology wrapper
- `adapters/openmm/ForceTerm.h` - Force object adapter
- `adapters/openmm/ContextState.h` - Context state wrapper

### Open Babel (conformer generation)
- `adapters/openbabel/OBForceFieldTerm.h` - Force field term
- `adapters/openbabel/OBConformerNeighborhood.h` - Conformer search

## Key Design Principles

1. **Separation of topology from state**: `ISystem` (topology: atoms, bonds, constraints) vs `IState` (coordinates, velocities). **Design assumption**: Topology is fixed during a search/simulation run (appropriate for conformation search, standard MD, protein design; not for reactive MD with bond breaking/forming). State is mutable. This matches OpenMM (System vs Context), GROMACS (Topology vs SimulationState), HOOMD-blue (SystemDefinition vs System). See [INTERFACE_DESIGN.md](INTERFACE_DESIGN.md#separation-of-physics-energy-from-state-space) for package-by-package mapping.

2. **State vs Move separation**: `IState` holds coordinates; `IMove` describes a change (what changed, where). Separation enables:
   - Incremental evaluation: `delta_energy(old_state, new_state, move)` uses move's locality metadata (`affected_atoms()`, `affected_residues()`) to compute energy deltas without full re-evaluation
   - Search algorithms: Generate moves, evaluate deltas, apply moves without copying full state
   - Ecosystem precedent: OSPREY uses `RotamerMove` (position + rotamer index) separate from rotamer assignments; Rosetta uses `Mover` objects that transform `Pose` states

3. **Capability query pattern**: Interfaces declare optional capabilities (`supports_delta_energy()`, `supports_lower_bound()`) for algorithm optimization

4. **Adapter pattern**: Each package adapter wraps native types to conform to `next` interfaces

5. **Composable energy terms**: `IEnergyTerm` building blocks compose into `IEnergyEvaluator`

## Implementation Status

- Core interfaces: Complete (header-only, C++17)
- OSPREY adapters: Stub implementations
- Rosetta/OpenMM/Open Babel adapters: Stub implementations
- Demo executable: [`demo/main_all_adapters.cpp`](demo/main_all_adapters.cpp) - Demonstrates all adapters

## Build

```bash
cd interfaces
cmake -B build -S .
cmake --build build --target interface-demo-all
./build/interface-demo-all
```

## Design Rationale

### Why is topology "immutable"?

**Design assumption**: The interfaces assume fixed topology during a search/simulation run. This is appropriate for:
- **Conformation search** (OSPREY, Rosetta): Exploring rotamers/angles with fixed sequence and connectivity
- **Standard MD** (OpenMM, GROMACS): Fixed-topology simulations (no reactive force fields)
- **Protein design**: Fixed backbone, varying side chains

**Not supported**: Reactive MD (bond breaking/forming, chemical reactions). For reactive simulations, topology changes would require different design (e.g., `ISystem` mutation methods or event-driven topology updates).

**Topology definition**: `ISystem` represents connectivity/structure: atom identities, bonds, constraints, dimensionality. During a search/simulation run, this structure is fixed. Coordinates (`IState`) change; topology does not.

**Ecosystem precedent**:
- **OpenMM**: `System` (topology + Force objects) is immutable; `Context` (positions/velocities) is mutable
- **GROMACS**: `Topology` defines system; `SimulationState` holds coordinates
- **OSPREY**: `ConfSpace` (conformation space definition) is fixed; rotamer assignments vary

**Benefits of fixed-topology assumption**:
- Thread safety: Multiple threads can share one `ISystem`, each with their own `IState`
- Caching: Energy terms can cache topology-dependent computations
- Algorithm efficiency: Search algorithms assume fixed dimensionality

### Why separate State and Move?

`IState` is the full state (coordinates for all atoms). `IMove` is a small descriptor of a change (e.g., "position 5 → rotamer 3" or "atoms 10-15 displaced by vector v").

**Ecosystem precedent:**
- **OSPREY**: `RotamerMove` is `(position, new_rotamer_index)` - a 2-tuple, not the full rotamer assignment vector. See [`adapters/osprey/RotamerMove.h`](adapters/osprey/RotamerMove.h)
- **Rosetta**: `Mover` objects are separate from `Pose` states. Movers transform poses; they don't contain the pose. See [`adapters/rosetta/MoverNeighborhood.h`](adapters/rosetta/MoverNeighborhood.h)

**Benefits:**
1. **Incremental evaluation**: `delta_energy(old_state, new_state, move)` uses `move.affected_atoms()` to compute only affected energy terms. Without move locality, you'd need full re-evaluation.
2. **Memory efficiency**: Moves are small (2-tuples, vectors of indices). Generating thousands of moves doesn't require copying full states.
3. **Search algorithm patterns**: A* generates moves, evaluates deltas, applies best move. Move descriptors enable this without state copying.

See [`include/next/IEnergyTerm.h`](include/next/IEnergyTerm.h) line 36: `delta_energy()` signature requires both states AND a move for locality metadata.

## Approach

1. **Start with structure**: Read core interface headers first (`include/next/*.h`), then adapters
2. **Use design docs as reference**: `INTERFACE_DESIGN.md` explains why, not just what
3. **Focus on one package**: Pick one adapter (e.g., OSPREY) and trace through its implementation
4. **Skip ecosystem references initially**: `ECOSYSTEM_REFERENCES.md` is a reading list, not required reading
5. **Run demo**: `interface-demo-all` shows concrete usage patterns
