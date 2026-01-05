# Python Bindings Interface Analysis

## Current Architecture Layers

The C++ kstar port has three distinct interface layers:

### Layer 1: Low-Level Components
- `EnergyMatrix<T>`: Stores pre-computed pairwise energies
- `PartitionFunction<T>`: Computes partition functions using A* search
- `AStarSearch`: Graph search implementation
- `EnergyMatrixLoader<T>`: Loads `.emat.bin` files exported from Java

### Layer 2: Mid-Level Workflow
- `KStarWorkflow<T>`: Takes three `EnergyMatrix` instances (protein/ligand/complex), computes partition functions, combines into K* score
- Input: Pre-loaded energy matrices
- Output: `KStarWorkflowResult` with log10 bounds and convergence status

### Layer 3: High-Level Parallel Computation
- `KStarParallel<T>`: Takes `ConfSpace` objects and sequences, computes K* scores in parallel
- Requires `ConfSpace` implementation (not yet ported to C++)
- Handles sequence iteration and thread pool management

## Current Python Integration

Existing Python code (`src/main/python/CCKStar/run_kstar_python.py`):
- Uses JPype to call Java OSPREY
- Loads `.ccsx` files via Java `ConfSpace.fromBytes()`
- Orchestrates K* runs via Java API
- Exports energy matrices from Java to `.emat.bin` format

## Binding Interface Options

### Option 1: Low-Level Bindings (EnergyMatrix + PartitionFunction)

**Interface:**
```python
import kstar_cpp

# Load energy matrices
protein_emat = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand_emat = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex_emat = kstar_cpp.load_energy_matrix("complex.emat.bin")

# Compute partition functions individually
pfunc = kstar_cpp.PartitionFunction()
protein_pfunc = pfunc.compute(protein_emat, epsilon=0.99)
ligand_pfunc = pfunc.compute(ligand_emat, epsilon=0.99)
complex_pfunc = pfunc.compute(complex_emat, epsilon=0.99)

# Combine manually
log10_kstar_lower = complex_pfunc.lower_bound - protein_pfunc.upper_bound - ligand_pfunc.upper_bound
log10_kstar_upper = complex_pfunc.upper_bound - protein_pfunc.lower_bound - ligand_pfunc.lower_bound
```

**Pros:**
- Maximum flexibility
- Exposes all primitives for custom workflows
- Good for testing/debugging individual components

**Cons:**
- Python must implement K* score combination logic (duplicates C++ code)
- More verbose API
- Error-prone (manual bounds arithmetic)

### Option 2: Mid-Level Bindings (KStarWorkflow + EnergyMatrixLoader)

**Interface:**
```python
import kstar_cpp

# Load energy matrices
protein_emat = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand_emat = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex_emat = kstar_cpp.load_energy_matrix("complex.emat.bin")

# Compute K* workflow
workflow = kstar_cpp.KStarWorkflow()
result = workflow.compute(
    protein=protein_emat,
    ligand=ligand_emat,
    complex=complex_emat,
    epsilon=0.99
)

# Access results
print(f"log10(K*) = {result.log10_value}")
print(f"Bounds: [{result.log10_lower_bound}, {result.log10_upper_bound}]")
print(f"Converged: {result.converged}")
```

**Pros:**
- Matches C++ workflow abstraction
- Encapsulates K* score computation (no manual bounds math)
- Clean, high-level API
- Works with existing `.emat.bin` export from Java
- No dependency on `ConfSpace` (not yet implemented)

**Cons:**
- Python must still load energy matrices (but loader exists)
- Cannot access individual partition function results without exposing them

### Option 3: High-Level Bindings (KStarParallel)

**Interface:**
```python
import kstar_cpp

# Load ConfSpace objects (requires ConfSpace implementation)
protein_cs = kstar_cpp.load_confspace("protein.ccsx")
ligand_cs = kstar_cpp.load_confspace("ligand.ccsx")
complex_cs = kstar_cpp.load_confspace("complex.ccsx")

# Define sequences
sequences = [
    kstar_cpp.Sequence([0, 1, 2, 3]),
    kstar_cpp.Sequence([1, 0, 2, 3]),
]

# Compute K* scores in parallel
kstar = kstar_cpp.KStarParallel()
results = kstar.compute(
    sequences=sequences,
    protein_confspace=protein_cs,
    ligand_confspace=ligand_cs,
    complex_confspace=complex_cs,
    epsilon=0.99,
    num_threads=8
)
```

**Pros:**
- Highest-level API (matches Java usage pattern)
- Handles parallelism automatically
- Works directly with `.ccsx` files

**Cons:**
- Requires `ConfSpace` implementation (not yet ported)
- Blocks on full port completion
- Less flexible for custom workflows

### Option 4: Hybrid Approach (Recommended for Phase 1)

**Interface:**
```python
import kstar_cpp

# Load energy matrices (from Java export)
protein_emat = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand_emat = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex_emat = kstar_cpp.load_energy_matrix("complex.emat.bin")

# Option A: Use workflow (recommended)
workflow = kstar_cpp.KStarWorkflow()
result = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=0.99)

# Option B: Access individual partition functions if needed
pfunc = kstar_cpp.PartitionFunction()
protein_pfunc = pfunc.compute(protein_emat, epsilon=0.99)
# ... etc
```

**Implementation:**
- Bind `KStarWorkflow<T>` as primary interface
- Bind `EnergyMatrixLoader<T>` for loading `.emat.bin` files
- Optionally expose `PartitionFunction<T>` for advanced use cases
- Expose result types (`KStarWorkflowResult`, `PartitionFunctionResult`)

**Pros:**
- Works immediately (no `ConfSpace` dependency)
- Matches current Java export workflow
- Provides both high-level and low-level access
- Can evolve to Option 3 when `ConfSpace` is ready

**Cons:**
- Requires Java to export `.emat.bin` files (but this already exists)

## Binding Technology Options

### pybind11 (Recommended)

**Pros:**
- Modern C++ template-friendly API
- Automatic type conversions (Python list ↔ C++ vector)
- Exception translation
- Memory management handled automatically
- Good performance (minimal overhead)
- Widely used in scientific computing

**Cons:**
- Requires C++17+ (already satisfied)
- Build-time dependency (header-only, but needs Python dev headers)

**Example:**
```cpp
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "kstar_workflow.hpp"

namespace py = pybind11;

PYBIND11_MODULE(kstar_cpp, m) {
    py::class_<osprey::kstar::EnergyMatrix<double>>(m, "EnergyMatrix")
        .def(py::init<>());

    py::class_<osprey::kstar::KStarWorkflowResult<double>>(m, "KStarWorkflowResult")
        .def_readonly("log10_value", &osprey::kstar::KStarWorkflowResult<double>::log10_value)
        .def_readonly("log10_lower_bound", &osprey::kstar::KStarWorkflowResult<double>::log10_lower_bound)
        .def_readonly("log10_upper_bound", &osprey::kstar::KStarWorkflowResult<double>::log10_upper_bound)
        .def_readonly("converged", &osprey::kstar::KStarWorkflowResult<double>::converged);

    py::class_<osprey::kstar::KStarWorkflow<double>>(m, "KStarWorkflow")
        .def(py::init<>())
        .def("compute", &osprey::kstar::KStarWorkflow<double>::compute);

    m.def("load_energy_matrix", &osprey::kstar::EnergyMatrixLoader<double>::loadFromFile);
}
```

### ctypes

**Pros:**
- No build-time dependencies (uses C ABI)
- Works with any C++ compiler
- Standard library (no external deps)

**Cons:**
- Requires manual C wrapper layer (`extern "C"`)
- Manual memory management
- No automatic type conversions
- More verbose Python code
- Error-prone (manual pointer management)

**Example:**
```cpp
// C wrapper
extern "C" {
    struct KStarWorkflowResult {
        double log10_value;
        double log10_lower_bound;
        double log10_upper_bound;
        bool converged;
    };

    KStarWorkflowResult* kstar_workflow_compute(
        const char* protein_emat_path,
        const char* ligand_emat_path,
        const char* complex_emat_path,
        double epsilon
    );
}
```

### CFFI

**Pros:**
- More Pythonic than ctypes
- Can generate bindings from C headers
- Good performance

**Cons:**
- Still requires C wrapper layer
- Less convenient than pybind11 for C++ templates

## Recommended Approach: Option 4 with pybind11

### Phase 1 Implementation

1. **Create pybind11 module** (`kstar_cpp`):
   - Bind `EnergyMatrixLoader<double>::loadFromFile`
   - Bind `KStarWorkflow<double>`
   - Bind `KStarWorkflowResult<double>`
   - Optionally bind `PartitionFunction<double>` for advanced use

2. **Python usage pattern:**
   ```python
   import kstar_cpp
   
   # Load energy matrices (exported from Java)
   protein = kstar_cpp.load_energy_matrix("protein.emat.bin")
   ligand = kstar_cpp.load_energy_matrix("ligand.emat.bin")
   complex = kstar_cpp.load_energy_matrix("complex.emat.bin")
   
   # Compute K* score
   workflow = kstar_cpp.KStarWorkflow()
   result = workflow.compute(protein, ligand, complex, epsilon=0.99)
   
   print(f"K* = 10^{result.log10_value}")
   print(f"Bounds: [10^{result.log10_lower_bound}, 10^{result.log10_upper_bound}]")
   ```

3. **Integration with existing Python code:**
   - Replace Java K* calls with C++ bindings
   - Keep Java for `.ccsx` loading and energy matrix export
   - Gradually migrate to full C++ when `ConfSpace` is ready

### Future Evolution (Phase 2+)

When `ConfSpace` is implemented:
- Add `ConfSpace` bindings
- Add `KStarParallel` bindings
- Support direct `.ccsx` loading in C++
- Eliminate Java dependency for K* computation

## Implementation Notes

### CMake Integration

Add pybind11 to `CMakeLists.txt`:
```cmake
find_package(pybind11 REQUIRED)

pybind11_add_module(kstar_cpp
    python_bindings.cpp
)

target_link_libraries(kstar_cpp PRIVATE
    kstar_lib
    pybind11::module
)
```

### Template Instantiation

pybind11 requires explicit template instantiation. Bind `double` specialization first:
- `EnergyMatrix<double>`
- `KStarWorkflow<double>`
- `PartitionFunction<double>`

Add `float` later if needed.

### Error Handling

pybind11 automatically translates C++ exceptions to Python exceptions. Ensure all C++ code throws `std::runtime_error` or `std::invalid_argument` for clear Python error messages.

### Memory Management

pybind11 handles memory automatically via smart pointers. For `EnergyMatrix`, use `std::shared_ptr` or return by value (pybind11 copies efficiently for small structs).

## Questions to Resolve

1. **Float vs Double**: Bind both or only `double`? (Recommend: start with `double`, add `float` if needed)

2. **Result Type Exposure**: Expose `PartitionFunctionResult` in workflow result, or keep it internal?

3. **Progress Reporting**: Add callback for progress updates during long computations?

4. **Thread Safety**: Document thread safety guarantees (workflow instances are stateless, safe to call from multiple threads)

5. **Build Integration**: Integrate with existing Python setup.py, or provide separate CMake build?

## Build Instructions

### Prerequisites

- CMake 3.20+
- C++20 compatible compiler (GCC 10+, Clang 12+, MSVC 2019+)
- Python 3.7+ with development headers
- pybind11 (will be fetched automatically if not found)

### Building the Python Module

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

### Installing the Module

**Option 1: Add to PYTHONPATH**
```bash
export PYTHONPATH="${REPO_ROOT}/build/cpp/kstar-python:$PYTHONPATH"
python3 -c "import kstar_cpp; print(kstar_cpp.__doc__)"
```

**Option 2: Copy to site-packages**
```bash
cp build/cpp/kstar-python/kstar_cpp*.so $(python3 -m site --user-site)/
```

**Option 3: Use setup.py (future)**
Create a setup.py that builds the module via CMake.

### Testing the Bindings

```python
import kstar_cpp

# Load energy matrices
protein = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex = kstar_cpp.load_energy_matrix("complex.emat.bin")

# Compute K* score
workflow = kstar_cpp.KStarWorkflow()
result = workflow.compute(protein, ligand, complex, epsilon=0.99)

print(f"K* = 10^{result.log10_value}")
```

See `python_bindings_example.py` for a complete example.

## Implementation Status

✅ **Completed:**
- pybind11 module structure
- `EnergyMatrixLoader<double>::loadFromFile` binding
- `KStarWorkflow<double>` binding
- `KStarWorkflowResult<double>` binding with `pfuncs` access
- `PartitionFunctionResult<double>` binding
- `PartitionFunction<double>` binding (optional, for advanced use)
- CMake integration with `KSTAR_ENABLE_PYTHON_BINDINGS` option
- Example Python script

**Files:**
- `src/main/cpp/kstar/python_bindings.cpp` - pybind11 bindings
- `src/main/cpp/kstar/python_bindings_example.py` - usage example
- `src/main/cpp/kstar/CMakeLists.txt` - build integration
