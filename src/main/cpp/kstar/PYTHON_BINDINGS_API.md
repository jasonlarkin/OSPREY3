# Python Bindings API Reference

## Overview

The `kstar_cpp` module provides Python bindings for the OSPREY C++ K* implementation. It allows you to compute K* scores using pre-computed energy matrices without requiring Java.

## Installation

### Building the Module

```bash
cd $REPO_ROOT

# Configure with Python bindings enabled
cmake -S src/main/cpp/kstar -B build/cpp/kstar-python \
  -DKSTAR_ENABLE_PYTHON_BINDINGS=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=ON

# Build
cmake --build build/cpp/kstar-python -j

# The module will be built as:
# - Linux/Mac: build/cpp/kstar-python/kstar_cpp.cpython-<version>-<arch>.so
# - Windows: build/cpp/kstar-python/kstar_cpp.pyd
```

### Setting PYTHONPATH

```bash
export PYTHONPATH="/path/to/osprey-fork_modern/build/cpp/kstar-python:$PYTHONPATH"
```

Or add to your Python script:
```python
import sys
sys.path.insert(0, "/path/to/osprey-fork_modern/build/cpp/kstar-python")
```

## Module Reference

### Functions

#### `load_energy_matrix(filepath: str) -> EnergyMatrix`

Load an energy matrix from a binary file exported by Java OSPREY.

**Parameters:**
- `filepath` (str): Path to the `.emat.bin` file

**Returns:**
- `EnergyMatrix`: Loaded energy matrix object

**Raises:**
- `RuntimeError`: If the file cannot be opened or is invalid

**Example:**
```python
import kstar_cpp
emat = kstar_cpp.load_energy_matrix("protein.emat.bin")
print(f"Positions: {emat.get_num_positions()}")
```

### Classes

#### `EnergyMatrix`

Represents a pre-computed energy matrix for partition function calculation.

**Methods:**

- `get_num_positions() -> int`: Returns the number of design positions
- `get_num_confs_at_pos(pos: int) -> int`: Returns the number of conformations at a specific position
- `get_const_term() -> float`: Returns the constant energy term (offset added to all energies)

**Example:**
```python
emat = kstar_cpp.load_energy_matrix("protein.emat.bin")
num_pos = emat.get_num_positions()
for pos in range(num_pos):
    num_confs = emat.get_num_confs_at_pos(pos)
    print(f"Position {pos}: {num_confs} conformations")
```

#### `KStarWorkflow`

Main class for computing K* scores from energy matrices.

**Methods:**

- `compute(protein: EnergyMatrix, ligand: EnergyMatrix, complex: EnergyMatrix, epsilon: float) -> KStarWorkflowResult`
- `compute(protein: EnergyMatrix, ligand: EnergyMatrix, complex: EnergyMatrix, epsilon: float, method: PartitionFunctionMethod) -> KStarWorkflowResult`
- `compute(protein: EnergyMatrix, ligand: EnergyMatrix, complex: EnergyMatrix, epsilon: float, method: PartitionFunctionMethod, options: PartitionFunctionOptions) -> KStarWorkflowResult`

**Parameters:**
- `protein` (EnergyMatrix): Protein energy matrix
- `ligand` (EnergyMatrix): Ligand energy matrix
- `complex` (EnergyMatrix): Complex energy matrix
- `epsilon` (float): Convergence threshold (0 < epsilon < 1). Lower values = faster but less accurate. Typical: 0.99
- `method` (PartitionFunctionMethod, optional): Computation method. Default: `PartitionFunctionMethod.AStar`
- `options` (PartitionFunctionOptions, optional): Computation options. Default: empty options

**Returns:**
- `KStarWorkflowResult`: Result containing K* score and partition function details

**Example:**
```python
workflow = kstar_cpp.KStarWorkflow()

# Simple usage
result = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=0.99)

# With method
result = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=0.99,
                          method=kstar_cpp.PartitionFunctionMethod.AStar)

# With options
options = kstar_cpp.PartitionFunctionOptions()
options.astar_variant = kstar_cpp.AStarVariant.Fast
result = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=0.99,
                          method=kstar_cpp.PartitionFunctionMethod.AStar,
                          options=options)
```

#### `KStarWorkflowResult`

Result of a K* workflow computation.

**Attributes:**

- `log10_value` (float): Point estimate of log10(K*)
- `log10_lower_bound` (float): Lower bound of log10(K*)
- `log10_upper_bound` (float): Upper bound of log10(K*)
- `converged` (bool): Whether all partition functions converged
- `pfuncs` (KStarPfuncTriplet): Individual partition function results

**Example:**
```python
result = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=0.99)
print(f"K* = 10^{result.log10_value}")
print(f"Bounds: [10^{result.log10_lower_bound}, 10^{result.log10_upper_bound}]")
print(f"Converged: {result.converged}")

# Access individual partition functions
print(f"Protein log10(Q): [{result.pfuncs.protein.lower_bound}, {result.pfuncs.protein.upper_bound}]")
```

#### `KStarPfuncTriplet`

Container for three partition function results (protein, ligand, complex).

**Attributes:**

- `protein` (PartitionFunctionResult): Protein partition function result
- `ligand` (PartitionFunctionResult): Ligand partition function result
- `complex` (PartitionFunctionResult): Complex partition function result

#### `PartitionFunctionResult`

Result of a partition function computation.

**Attributes:**

- `lower_bound` (float): log10 partition function lower bound
- `upper_bound` (float): log10 partition function upper bound
- `delta` (float): Convergence gap: (upper - lower) / upper
- `num_confs` (int): Number of conformations explored
- `converged` (bool): Whether delta <= epsilon

**Example:**
```python
result = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=0.99)
pfunc_result = result.pfuncs.protein
print(f"Explored {pfunc_result.num_confs} conformations")
print(f"Delta: {pfunc_result.delta}")
print(f"Converged: {pfunc_result.converged}")
```

#### `PartitionFunction`

Low-level partition function calculator (optional, for advanced use).

**Methods:**

- `compute(energy_matrix: EnergyMatrix, epsilon: float) -> PartitionFunctionResult`
- `compute(energy_matrix: EnergyMatrix, epsilon: float, method: PartitionFunctionMethod) -> PartitionFunctionResult`
- `compute(energy_matrix: EnergyMatrix, epsilon: float, method: PartitionFunctionMethod, options: PartitionFunctionOptions) -> PartitionFunctionResult`

**Example:**
```python
pfunc = kstar_cpp.PartitionFunction()
protein_pfunc = pfunc.compute(protein_emat, epsilon=0.99)
print(f"Protein log10(Q): [{protein_pfunc.lower_bound}, {protein_pfunc.upper_bound}]")
```

#### `PartitionFunctionOptions`

Options for partition function computation.

**Attributes:**

- `allow_exact_enumeration` (bool): Allow exact enumeration for small spaces (default: True)
- `astar_variant` (AStarVariant): A* implementation variant (default: AStarVariant.Baseline)

**Example:**
```python
options = kstar_cpp.PartitionFunctionOptions()
options.allow_exact_enumeration = False
options.astar_variant = kstar_cpp.AStarVariant.Fast
```

### Enumerations

#### `PartitionFunctionMethod`

Computation method for partition function.

- `AStar`: Use A* search algorithm (default)
- `GradientDescent`: Use gradient descent algorithm

#### `AStarVariant`

A* implementation variant.

- `Baseline`: Reference implementation (default)
- `Fast`: Optimized implementation

## Usage Examples

### Basic K* Computation

```python
import kstar_cpp

# Load energy matrices
protein = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex_emat = kstar_cpp.load_energy_matrix("complex.emat.bin")

# Compute K* score
workflow = kstar_cpp.KStarWorkflow()
result = workflow.compute(protein, ligand, complex_emat, epsilon=0.99)

# Display results
print(f"K* = 10^{result.log10_value:.4f}")
print(f"Bounds: [10^{result.log10_lower_bound:.4f}, 10^{result.log10_upper_bound:.4f}]")
print(f"Converged: {result.converged}")
```

### Individual Partition Functions

```python
import kstar_cpp

# Load energy matrix
emat = kstar_cpp.load_energy_matrix("protein.emat.bin")

# Compute partition function
pfunc = kstar_cpp.PartitionFunction()
result = pfunc.compute(emat, epsilon=0.99)

print(f"log10(Q) = [{result.lower_bound:.4f}, {result.upper_bound:.4f}]")
print(f"Explored {result.num_confs} conformations")
print(f"Delta: {result.delta:.6f}")
```

### Using Fast A* Variant

```python
import kstar_cpp

# Load energy matrices
protein = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex_emat = kstar_cpp.load_energy_matrix("complex.emat.bin")

# Configure options
options = kstar_cpp.PartitionFunctionOptions()
options.astar_variant = kstar_cpp.AStarVariant.Fast
options.allow_exact_enumeration = False

# Compute with options
workflow = kstar_cpp.KStarWorkflow()
result = workflow.compute(protein, ligand, complex_emat, epsilon=0.99,
                          method=kstar_cpp.PartitionFunctionMethod.AStar,
                          options=options)
```

### Batch Processing Multiple Sequences

```python
import kstar_cpp
import glob

# Find all energy matrix triplets
protein_files = sorted(glob.glob("data/*/protein.emat.bin"))

results = []
for protein_file in protein_files:
    base = protein_file.replace("/protein.emat.bin", "")
    ligand_file = f"{base}/ligand.emat.bin"
    complex_file = f"{base}/complex.emat.bin"
    
    # Load matrices
    protein = kstar_cpp.load_energy_matrix(protein_file)
    ligand = kstar_cpp.load_energy_matrix(ligand_file)
    complex_emat = kstar_cpp.load_energy_matrix(complex_file)
    
    # Compute K*
    workflow = kstar_cpp.KStarWorkflow()
    result = workflow.compute(protein, ligand, complex_emat, epsilon=0.99)
    
    results.append({
        'file': base,
        'kstar': 10**result.log10_value,
        'converged': result.converged
    })

# Print results
for r in results:
    print(f"{r['file']}: K* = {r['kstar']:.2e}, converged={r['converged']}")
```

## Thread Safety

- `KStarWorkflow` instances are stateless and thread-safe. Multiple threads can use the same workflow instance concurrently.
- `PartitionFunction` instances are stateless and thread-safe.
- `EnergyMatrix` objects are read-only after loading and are safe to share across threads.

**Example (parallel processing):**
```python
import kstar_cpp
from concurrent.futures import ThreadPoolExecutor

def compute_kstar(args):
    protein_file, ligand_file, complex_file = args
    protein = kstar_cpp.load_energy_matrix(protein_file)
    ligand = kstar_cpp.load_energy_matrix(ligand_file)
    complex_emat = kstar_cpp.load_energy_matrix(complex_file)
    
    workflow = kstar_cpp.KStarWorkflow()
    result = workflow.compute(protein, ligand, complex_emat, epsilon=0.99)
    return result.log10_value

# Process multiple files in parallel
files = [("p1.emat.bin", "l1.emat.bin", "c1.emat.bin"),
         ("p2.emat.bin", "l2.emat.bin", "c2.emat.bin")]

with ThreadPoolExecutor(max_workers=4) as executor:
    results = list(executor.map(compute_kstar, files))
```

## Error Handling

All C++ exceptions are automatically converted to Python exceptions:

- `RuntimeError`: File I/O errors, invalid data format
- `ValueError`: Invalid parameters (e.g., epsilon outside [0,1])

**Example:**
```python
try:
    emat = kstar_cpp.load_energy_matrix("nonexistent.emat.bin")
except RuntimeError as e:
    print(f"Error loading file: {e}")

try:
    result = workflow.compute(protein, ligand, complex_emat, epsilon=1.5)
except ValueError as e:
    print(f"Invalid epsilon: {e}")
```

## Performance Notes

- Computation time scales with conformation space size
- Typical runtime: seconds to minutes per partition function
- Memory usage: proportional to energy matrix size
- For faster results, use `epsilon=0.95` instead of `0.99` (less accurate but faster)
- Fast A* variant (`AStarVariant.Fast`) is typically 2-5x faster than Baseline

## Limitations

- Requires pre-computed `.emat.bin` files from Java OSPREY
- No direct `.ccsx` file support (must export energy matrices from Java first)
- No progress callbacks (long computations block until complete)
- Only `double` precision currently supported (float support can be added)

## Troubleshooting

### ImportError: No module named 'kstar_cpp'

**Solution:** Add the build directory to PYTHONPATH:
```bash
export PYTHONPATH="/path/to/build/cpp/kstar-python:$PYTHONPATH"
```

Or verify the module was built:
```bash
ls build/cpp/kstar-python/kstar_cpp*.so
```

### RuntimeError: Cannot open file

**Solution:** Verify the file path is correct and the file exists:
```python
import os
print(os.path.exists("protein.emat.bin"))  # Should be True
```

### Very slow computation

**Possible causes:**
- Large conformation space (many conformations per position)
- Very strict epsilon (e.g., 0.999)
- Using Baseline A* instead of Fast variant

**Solutions:**
- Use `epsilon=0.95` for faster results
- Use `AStarVariant.Fast` in options
- Check energy matrix size: `emat.get_num_positions()` and conformations per position

### Results don't match Java implementation

**Possible causes:**
- Different epsilon values
- Numerical precision differences (C++ uses double, Java uses BigDecimal)
- Different A* implementation behavior

**Solutions:**
- Verify epsilon values match
- For exact matching, Java implementation may be required
- Differences < 0.1% in log10 space are typically acceptable

## See Also

- `python_bindings_example.py`: Complete usage example
- `test_python_bindings.py`: Basic API tests
- `visualize_kstar_results.py`: Visualization script
- `PYTHON_BINDINGS_ANALYSIS.md`: Design decisions and architecture
- `PYTHON_BINDINGS_NEXT_STEPS.md`: Future enhancements
