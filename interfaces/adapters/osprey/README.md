# OSPREY Adapter Implementation

Wraps OSPREY's existing components as generalized interfaces.

## Components

### `EnergyMatrixTerm`
- Wraps `edu.duke.cs.osprey.ematrix.EnergyMatrix` as `IEnergyTerm`
- Implements `delta_energy()` for efficient A* search
- Implements `lower_bound()` for branch-and-bound

### `RotamerConformation`
- Wraps rotamer assignment vector as `IConformation`
- Converts between rotamer indices and Cartesian coordinates (via OSPREY's coordinate assignment)

### `RotamerMove`
- Represents rotamer change at one or more positions
- Locality metadata: which residues are affected

### `RotamerNeighborhood`
- Generates moves from a rotamer conformation
- Uses OSPREY's `ConfSpace` to enumerate valid rotamer choices

## Integration Points

**Java → C++ bridge:**
- OSPREY is primarily Java; adapter needs JNI or equivalent
- Alternative: extract energy matrix computation to C++ and reimplement

**Key OSPREY classes to wrap:**
- `EnergyMatrix` → `EnergyMatrixTerm`
- `ConfSpace` → used by `RotamerNeighborhood`
- Rotamer assignment → `RotamerConformation`
- A* search → `ISearchAlgorithm` implementation (future)
