# Interface Design Documentation

This document describes the design of the generalized `next` interfaces, how they handle diverse external packages, and the features used.

## Overview

The `next` interfaces provide a unified abstraction layer for molecular energy evaluation and conformation search across different simulation packages. Each package has its own design philosophy and representation, but all share common concepts: molecular systems, energy evaluation, and state exploration.

## Separation of Physics (Energy) from State Space

A key design principle is separating **physics models** (how energy is computed) from **state representations** (what coordinates/conformations are being evaluated). This separation enables:
- Swappable energy backends (classical force fields, quantum mechanical methods, hybrid models)
- Multiple state representations (Cartesian coordinates, internal coordinates, discrete rotamers, wavefunctions)
- Algorithm generality (search algorithms operate on states, energy evaluation operates on physics models)

### Classical MD Packages: Strong Separation

**OpenMM**: 
- **Physics**: `System` (topology + `Force` objects defining energy terms)
- **State**: `Context` (runtime positions, velocities, parameters)
- Clear separation: System is immutable configuration, Context holds mutable state

**GROMACS**:
- **Physics**: `Topology` (topology + interaction maps)
- **State**: `SimulationState` (coordinates, velocities)
- Clear separation: Topology defines the system, SimulationState holds coordinates

**HOOMD-blue**:
- **Physics**: `SystemDefinition` (particles, bonds, topology)
- **State**: `System` (positions, velocities, forces)
- Clear separation: Definition is configuration, System is runtime state

**LAMMPS**:
- **Physics**: `Pair`/`Fix`/`Compute` objects (orchestrated by `Modify`)
- **State**: Implicit simulation state (positions stored in atom arrays)
- Less clear: State is implicit, but physics objects are explicit and composable

### Protein Design Packages: Conceptual Separation

**Rosetta**:
- **Physics**: `ScoreFunction` + `EnergyMethod` terms (weighted scoring)
- **State**: `Pose` with `Conformation` (structure) and `Energies` (cached scores)
- Hybrid: Pose contains both, but `ScoreFunction.score(Pose&)` separates evaluation from state

**OSPREY**:
- **Physics**: `EnergyMatrix` (precomputed pairwise energies)
- **State**: Rotamer assignments (discrete conformation vector)
- Clear separation: EnergyMatrix is immutable lookup table, rotamer assignments are mutable state

### Quantum Mechanical Packages: Blurred Separation

**Psi4**:
- **Physics**: Method type (SCF, MP2, CCSD, etc.) + `BasisSet` (AO basis definition)
- **State**: `Wavefunction` object contains both:
  - Physics model: Method type, basis set, Hamiltonian operator
  - State representation: Molecular orbitals, density matrix, Fock matrix
- **Challenge**: Wavefunction objects couple physics model with quantum state. Energy evaluation (`compute_energy()`) operates on the wavefunction, which contains both aspects.

**Implications for `next` interfaces**:
- For QM backends, `IEnergyTerm::evaluate(const IState&)` may need to accept a QM-aware `IState` that carries wavefunction/density matrix
- Alternatively: QM state (wavefunction) could be cached within the `IEnergyTerm` adapter, treating it as an internal detail
- QM adapters may need to extend `IState` with quantum-specific methods (`get_density_matrix()`, `get_orbitals()`, etc.)

**Recommendation**: For QM packages, treat the `Wavefunction` as a hybrid object that the adapter manages internally. The `IState` passed to `evaluate()` provides molecular geometry, and the adapter maintains quantum state internally (with caching/reuse strategies).

## Core Interfaces

### System Representation

#### `ISystem`
- **Purpose**: Molecular topology and composition (atoms, bonds, constraints, energy terms)
- **Pattern**: Separates topology from runtime state
- **Package Adaptations**:
  - **OpenMM**: `System` → `SystemWrapper` (owns particles, constraints, Force objects)
  - **GROMACS**: `Topology` → wrapper maps directly
  - **Rosetta**: `Pose` contains both topology and state, but adapter extracts topology aspects
  - **OSPREY**: Conformation space definition (ConfSpace) provides topology

#### `IState`
- **Purpose**: Runtime molecular state (coordinates, velocities, cached data)
- **Pattern**: Binds to an `ISystem` and provides thread-safe state access
- **Package Adaptations**:
  - **OpenMM**: `Context` → `ContextState` (positions, velocities, parameters)
  - **GROMACS**: `SimulationState` → wrapper maps coordinates/velocities
  - **Rosetta**: `Pose` → `PoseState` (structure + annotations)
  - **OSPREY**: Rotamer assignments encoded as discrete state

### Energy Evaluation

#### `IEnergyTerm`
- **Purpose**: Single energy contribution (composable building block)
- **Pattern**: Declarative capabilities (delta_energy, lower_bound) for algorithm efficiency
- **Package Adaptations**:
  - **OpenMM**: `Force` objects → `ForceTerm` (HarmonicBondForce, NonbondedForce, etc.)
  - **Rosetta**: `EnergyMethod` objects → individual adapters per term type
  - **OSPREY**: `EnergyMatrix` → `EnergyMatrixTerm` (precomputed pairwise energies)
  - **Open Babel**: `OBForceField` → `OBForceFieldTerm` (MM force field)
  - **LAMMPS**: `Pair`/`Fix`/`Compute` → individual term adapters

#### `IEnergyEvaluator`
- **Purpose**: Composed energy evaluator (sum of multiple terms)
- **Pattern**: Aggregator that delegates to terms and checks capability consistency
- **Package Adaptations**:
  - **OpenMM**: System's `vector<Force*>` evaluated by Platform
  - **Rosetta**: `ScoreFunction` → `ScoreFunctionEvaluator` (weighted EnergyMethod list)
  - **OSPREY**: Single EnergyMatrix (but could compose multiple terms)
  - **LAMMPS**: `Modify` orchestrates Pair/Fix/Compute lists

### Search/Sampling

#### `IConformation`
- **Purpose**: Compact conformation representation (DOF assignments)
- **Pattern**: Abstract state representation for search algorithms
- **Package Adaptations**:
  - **OSPREY**: `RotamerConformation` (discrete rotamer index vector)
  - **Open Babel**: RotorKey (torsion angle assignments)
  - **Rosetta**: Pose (Cartesian + internal coordinates)
  - **General MD**: Continuous coordinate vectors

#### `IMove`
- **Purpose**: Move descriptor (state mutation with locality metadata)
- **Pattern**: Explicit move representation for search algorithms
- **Package Adaptations**:
  - **OSPREY**: `RotamerMove` (single/multi-position rotamer changes)
  - **Rosetta**: Movers transform Pose states (packing, minimization, etc.)
  - **Open Babel**: Conformer search generates RotorKey variations
  - **General**: Atom displacements, dihedral rotations, etc.

#### `INeighborhood`
- **Purpose**: Neighborhood generator (moves from a state)
- **Pattern**: State space navigation
- **Package Adaptations**:
  - **OSPREY**: `RotamerNeighborhood` (all possible rotamer changes)
  - **Rosetta**: `MoverNeighborhood` (Mover objects generate neighbor states)
  - **Open Babel**: `OBConformerNeighborhood` (conformer search variations)
  - **General**: Local search, random walks, systematic enumeration

#### `ISearchProblem`
- **Purpose**: Search problem definition (composes neighborhood + evaluator + goal)
- **Pattern**: Problem formulation for search algorithms

#### `ISearchAlgorithm`
- **Purpose**: Abstract search algorithm
- **Pattern**: Generic search over `ISearchProblem`

## Features

1. **`std::unique_ptr<T>`** - Smart pointer ownership for terms, states, moves
   ```cpp
   virtual std::unique_ptr<IState> create_state() const = 0;
   std::vector<std::unique_ptr<IEnergyTerm>> terms_;
   ```

2. **Template argument deduction** - Simplified generic code
   ```cpp
   auto moves = std::make_unique<RotamerMove>(pos, rot);
   ```

3. **Structured bindings** - Not currently used, but available for future extensions


### Potential Features

- **Concepts** - Could define concepts for `EnergyTerm`, `State`, etc.
  ```cpp
  template<typename T>
  concept EnergyTerm = requires(T t, const IState& s) {
      { t.evaluate(s) } -> std::convertible_to<double>;
      t.supports_delta_energy();
  };
  ```

- **`std::ranges`** - Could simplify iteration over terms/moves
- **Coroutines** - Could enable lazy move generation for large neighborhoods
- **Modules** - Could replace header includes for faster compilation

- **`std::mdspan`** - Multi-dimensional arrays for coordinate storage
- **Deducing `this`** - Simplify const/non-const method overloads

## Design Patterns

### 1. Adapter Pattern
Each package adapter wraps native types to conform to the `next` interfaces:
- **Composition**: Adapters contain pointers/references to native objects
- **Forwarding**: Adapters forward calls to native implementations
- **Conversion**: Adapters convert between `next` types and native types

Example:
```cpp
class EnergyMatrixTerm : public IEnergyTerm {
    EnergyMatrix* emat_;  // Wraps OSPREY type
public:
    double evaluate(const IState& state) override {
        // Convert IState → OSPREY representation → evaluate
    }
};
```

### 2. Strategy Pattern
Search algorithms are interchangeable via `ISearchAlgorithm`:
- Different algorithms (A*, branch-and-bound, MCTS) implement the same interface
- Problem formulation (`ISearchProblem`) is algorithm-agnostic

### 3. Factory Pattern
State and term creation uses factory methods:
```cpp
std::unique_ptr<IState> create_state() const;  // Factory method
```

### 4. Capability Query Pattern
Interfaces declare capabilities rather than requiring all implementations:
```cpp
bool supports_delta_energy() const;  // Query capability
double delta_energy(...) = 0;        // Use capability (may throw if unsupported)
```


- **Efficient implementations**: OSPREY can provide fast incremental evaluation
- **Simple implementations**: Basic force fields can skip incremental support
- **Algorithm optimization**: Search algorithms can check capabilities and optimize paths

## How Interfaces Adapt to Package Designs

### OpenMM (Object-Oriented, Plugin Architecture)

**Design Philosophy**: Front-end/back-end separation, Platform-based evaluation

**Adaptation Strategy**:
- `System` → `SystemWrapper`: Wraps topology composition
- `Force` → `ForceTerm`: Each Force type adapts to `IEnergyTerm`
- `Context` → `ContextState`: Runtime state with backend binding

**Key Challenge**: OpenMM uses a Platform registry for evaluation. Adapter must:
- Create a Context from System + State
- Evaluate via Platform (not directly via Force objects)

### Rosetta (Mover Pipeline, ScoreFunction Composition)

**Design Philosophy**: Compositional operators (Movers) and weighted score terms

**Adaptation Strategy**:
- `ScoreFunction` → `ScoreFunctionEvaluator`: Wraps weighted term composition
- `Pose` → `PoseState`: Extracts state aspects from Pose
- `Mover` → `MoverNeighborhood`: Movers generate neighbor states

**Key Challenge**: Rosetta uses shared pointers extensively. Adapter must:
- Handle `utility::pointer::shared_ptr` lifetime management
- Convert between Pose and IState representations

### OSPREY (Precomputed Energies, Discrete Search)

**Design Philosophy**: Precompute pairwise energies, discrete rotamer space

**Adaptation Strategy**:
- `EnergyMatrix` → `EnergyMatrixTerm`: Precomputed matrix maps directly
- Rotamer assignments → `RotamerConformation`: Discrete representation
- Rotamer changes → `RotamerMove`: Explicit move descriptors

**Key Advantage**: OSPREY's design aligns well with `next` interfaces:
- `delta_energy()` is naturally supported (matrix lookup)
- `lower_bound()` is naturally supported (partial assignment bounds)

### Open Babel (C-style API, Conformer Search)

**Design Philosophy**: C++ wrapper around C API, conformer enumeration

**Adaptation Strategy**:
- `OBForceField` → `OBForceFieldTerm`: Wraps C++ API
- `OBConformerSearch` → `OBConformerNeighborhood`: Generates conformer moves

**Key Challenge**: Open Babel uses C-style error returns. Adapter must:
- Convert return codes to exceptions
- Manage object lifetime (C API requires explicit cleanup)

### LAMMPS (Scriptable, Event-Driven)

**Design Philosophy**: Scriptable simulation, event-driven execution hooks

**Adaptation Strategy**:
- `Pair`/`Fix`/`Compute` → individual `IEnergyTerm` adapters
- LAMMPS object → `ISystem` wrapper (orchestrates Pair/Fix/Compute lists)

**Key Challenge**: LAMMPS uses a scriptable input system. Adapter must:
- Extract configuration from input script or API calls
- Map LAMMPS's event-driven hooks to `next`'s explicit evaluation

## Type Safety and Ownership

### Ownership Models

**Question**: Who owns wrapped native objects?

**Current Approach**: Adapters take raw pointers (borrowed or owned? TBD)
```cpp
explicit EnergyMatrixTerm(EnergyMatrix* emat);  // Owned? Borrowed?
```

**Options**:
1. **Borrowed**: Caller retains ownership, adapter uses during lifetime
2. **Owned**: Adapter takes ownership via `std::unique_ptr`
3. **Shared**: Use `std::shared_ptr` for shared ownership


```cpp
explicit EnergyMatrixTerm(std::unique_ptr<EnergyMatrix> emat);  // Owned
// or
explicit EnergyMatrixTerm(EnergyMatrix& emat);  // Borrowed reference
```

### Type Conversions

**Challenge**: Converting between `IState` and native representations

**Current Approach**: Helper methods in adapters
```cpp
Pose* to_pose(const IState& state) const;  // Conversion helper
```

**Future Enhancement**: Use visitor pattern or type traits for safer conversions

## Thread Safety

**Current State**: Interfaces do not specify thread safety requirements

**Package Reality**:
- **OpenMM**: Context objects are thread-safe, System is immutable
- **Rosetta**: Pose objects are not thread-safe
- **OSPREY**: EnergyMatrix is read-only and thread-safe
- **LAMMPS**: Not thread-safe (single simulation instance)

**Recommendation**: Document thread safety requirements per interface:
- `ISystem`: Immutable after construction (read-only, thread-safe)
- `IState`: Not thread-safe (each thread needs own copy)
- `IEnergyTerm`: Immutable evaluation (thread-safe if state is thread-safe)

## Performance Considerations

### Virtual Function Overhead

**Concern**: Virtual function calls add overhead compared to direct calls

**Mitigation**:

- Most energy evaluation is compute-bound (virtual call overhead is negligible)
- Consider `final` on leaf classes to enable devirtualization (TBD)

### Incremental Evaluation

**Optimization**: `delta_energy()` enables efficient search algorithms

**Implementation**:
- OSPREY: Matrix lookup (O(1) per position)
- OpenMM: Force recalculation (O(n) affected particles)
- Rosetta: Incremental scoring if supported by EnergyMethod (ref)

**Algorithm Benefit**: A* search can use `delta_energy()` to avoid full re-evaluation

### Caching

**Pattern**: Interfaces do not mandate caching, but adapters can cache

**Example**: `IState` can cache computed energies:
```cpp
class CachedState : public IState {
    mutable double cached_energy_;
    mutable bool energy_valid_;
};
```

## Future Enhancements

### 1. Concepts 
Define concepts for compile-time interface checking:
```cpp
template<typename T>
concept EnergyTerm = requires(T t, const IState& s) {
    { t.evaluate(s) } -> std::convertible_to<double>;
};
```

### 2. Coroutines 
Lazy move generation for large neighborhoods:
```cpp
std::generator<std::unique_ptr<IMove>> generate_moves_async(const IConformation& conf);
```

### 3. Modules 
Replace headers with modules for faster compilation

### 4. Reflection 
Introspect interface capabilities at compile-time

## Capability Matrix

### Energy Evaluation Capabilities by Package

| Package | `evaluate()` | `delta_energy()` | `lower_bound()` | Notes |
|---------|-------------|------------------|-----------------|-------|
| **OSPREY** | Yes | Yes | Yes | Precomputed matrix enables efficient incremental evaluation |
| **Rosetta** | Yes | No | No | Full scoring each time, no incremental support |
| **OpenMM** | Yes | No | No | Force recalculation, no incremental support |
| **Open Babel** | Yes | No | No | Force field evaluation, no incremental support |
| **LAMMPS** | Yes | No | No | Pair/Fix evaluation, no incremental support |

### Search/Sampling Capabilities by Package

| Package | `IConformation` | `IMove` | `INeighborhood` | Search Type |
|---------|----------------|---------|-----------------|-------------|
| **OSPREY** | Yes (RotamerConformation) | Yes (RotamerMove) | Yes (RotamerNeighborhood) | Discrete (rotamer index) |
| **Rosetta** | Yes (PoseState) | Yes (Mover moves) | Yes (MoverNeighborhood) | Continuous/Discrete hybrid |
| **OpenMM** | Yes (ContextState) | Partial (Custom) | Partial (Custom) | Continuous (MD trajectories) |
| **Open Babel** | Yes (Conformer) | Yes (RotorKey) | Yes (OBConformerNeighborhood) | Torsion angle based |
| **LAMMPS** | Partial (Custom) | Partial (Custom) | Partial (Custom) | Continuous (MD simulations) |

**Legend**:
- Yes: Fully supported
- No: Not supported
- Partial: Partial support or requires custom implementation

## Implementation Status

### Current Implementation
- Core interfaces (`ISystem`, `IState`, `IEnergyTerm`, `IEnergyEvaluator`, `IConformation`, `IMove`, `INeighborhood`, `ISearchProblem`, `ISearchAlgorithm`)
- OSPREY adapters (EnergyMatrixTerm, RotamerConformation, RotamerMove, RotamerNeighborhood)
- Rosetta adapter stubs (ScoreFunctionEvaluator, PoseState, MoverNeighborhood)
- OpenMM adapter stubs (ForceTerm, SystemWrapper, ContextState)
- Open Babel adapter stubs (OBForceFieldTerm, OBConformerNeighborhood)
- Demo executable demonstrating all adapters

### Pending Implementation
- Complete adapter implementations (currently stubs with placeholder code)
- Search algorithm implementations (A*, branch-and-bound, MCTS, etc.)
- Integration with actual packages (requires package headers/libraries)
- Thread safety documentation and testing
- Performance benchmarking

## Summary

The `next` interfaces use features to provide a unified abstraction layer across diverse molecular simulation packages. The design adapts to each package's philosophy:

- **OpenMM**: Front-end/back-end separation → adapter bridges System/Context
- **Rosetta**: Compositional operators → adapter wraps Movers/ScoreFunction
- **OSPREY**: Precomputed discrete search → adapter maps naturally to interfaces
- **Open Babel**: C API wrapper → adapter handles C-style error returns

Key C++ features: `std::unique_ptr`, virtual functions, template deduction, smart pointers for ownership.

Key design patterns: Adapter, Strategy, Factory, Capability Query.

Future enhancements: concepts, coroutines, modules for improved type safety and performance.

### Requirements

- CMake 3.16 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- No external dependencies required (interfaces and adapters are header-only or use standard library only)

### Building and Running

Build the interfaces and demo:
```bash
cd interfaces
cmake -B build -S .
cmake --build build --target interface-demo-all
```

Run the demo:
```bash
./build/interface-demo-all
```

### Demo Results

Running `interface-demo-all` produces:

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

This demonstrates:
- **OSPREY** adapters fully support incremental evaluation (delta_energy, lower_bound) - optimal for A* search
- **Rosetta/OpenMM/Open Babel** adapters provide basic evaluation - suitable for full-state search algorithms
- All adapters successfully instantiate and can be used polymorphically through the `next` interfaces
