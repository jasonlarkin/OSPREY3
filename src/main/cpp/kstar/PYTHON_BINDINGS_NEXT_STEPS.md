# Python Bindings Next Steps and Visualization

## Immediate Next Steps

### 1. Test with Real Data

**Goal**: Validate bindings with actual .emat.bin files from Java OSPREY

**Steps**:
- Export .emat.bin files from Java OSPREY for a known test case
- Run `python_bindings_example.py` with real data
- Compare results with Java K* implementation
- Validate partition function convergence behavior

**Required**:
- Test .emat.bin files (protein, ligand, complex)
- Known expected K* scores for comparison

### 2. Add Progress Callbacks

**Current State**: No progress reporting during long computations

**Implementation Options**:

**Option A: Callback Function (Recommended)**
```cpp
// C++ side: Add to ComputeOptions
struct ComputeOptions {
    std::function<void(int64_t num_confs, T lower, T upper, T delta)> progress_callback;
};

// Python side: bind std::function
py::class_<PartitionFunction<double>::ComputeOptions>(m, "PartitionFunctionOptions")
    .def_readwrite("progress_callback", ...)
```

**Option B: Periodic Status Queries**
```python
# Python wrapper that periodically checks status
# Requires intermediate result storage (thread-safe)
```

**Option C: Python Generator/Iterator**
```python
# Yield intermediate results during computation
# Requires significant C++ refactoring
```

**Recommendation**: Start with Option A (callback), add at partition function computation checkpoints

### 3. Performance Benchmarking

**Goal**: Compare C++ bindings vs Java implementation

**Metrics to Track**:
- Computation time per partition function
- Memory usage
- Convergence rate (confs explored vs epsilon)
- Throughput (sequences per second)

**Tools**:
- Python `timeit` module
- `memory_profiler` for memory tracking
- Custom timing wrappers in Python

### 4. Integration with Existing Python Code

**Goal**: Replace Java K* calls with C++ bindings in existing OSPREY Python code

**Files to Modify**:
- `src/main/python/CCKStar/run_kstar_python.py`
- Other Python scripts that call Java K*

**Approach**:
- Keep Java for .ccsx loading and energy matrix export
- Replace K* computation calls with C++ bindings
- Maintain API compatibility where possible

### 5. Error Handling and Validation

**Current State**: Basic exception translation via pybind11

**Enhancements**:
- Validate epsilon range (0 < epsilon < 1)
- Validate energy matrix dimensions match
- Clear error messages for common failures
- Type checking in Python bindings

## Visualization Tools and Approaches

### 1. Partition Function Convergence Plotting

**What to Visualize**:
- Lower bound vs iterations
- Upper bound vs iterations
- Delta (convergence gap) vs iterations
- Number of conformations explored

**Implementation**:
```python
import matplotlib.pyplot as plt
import numpy as np

# With progress callback
def plot_convergence(protein_result, ligand_result, complex_result):
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    
    # Plot 1: Bounds over time
    axes[0, 0].plot(protein_result.lower_history, label='Lower')
    axes[0, 0].plot(protein_result.upper_history, label='Upper')
    axes[0, 0].set_xlabel('Iterations')
    axes[0, 0].set_ylabel('log10(Q)')
    axes[0, 0].set_title('Protein Partition Function Convergence')
    axes[0, 0].legend()
    axes[0, 0].grid(True)
    
    # Plot 2: Delta over time
    axes[0, 1].semilogy(protein_result.delta_history)
    axes[0, 1].axhline(y=0.01, color='r', linestyle='--', label='1% threshold')
    axes[0, 1].set_xlabel('Iterations')
    axes[0, 1].set_ylabel('Delta (log scale)')
    axes[0, 1].set_title('Convergence Delta')
    axes[0, 1].legend()
    axes[0, 1].grid(True)
    
    # Similar for ligand and complex...
```

**Required**: Progress callback implementation to collect history

### 2. Energy Matrix Heatmaps

**What to Visualize**:
- One-body energy distribution per position
- Pairwise energy matrix (2D heatmap)
- Energy distribution histograms

**Implementation**:
```python
import matplotlib.pyplot as plt
import seaborn as sns
import numpy as np

def plot_energy_matrix(emat, title="Energy Matrix"):
    num_pos = emat.get_num_positions()
    
    # Extract one-body energies
    one_body = []
    for pos in range(num_pos):
        num_confs = emat.get_num_confs_at_pos(pos)
        for conf in range(num_confs):
            one_body.append(emat.get_one_body(pos, conf))
    
    # Plot histogram
    plt.figure(figsize=(10, 6))
    plt.hist(one_body, bins=50, edgecolor='black')
    plt.xlabel('Energy (kcal/mol)')
    plt.ylabel('Frequency')
    plt.title(f'{title} - One-Body Energy Distribution')
    plt.grid(True, alpha=0.3)
    
    # Plot pairwise matrix (sample for small matrices)
    if num_pos <= 20:
        pairwise_matrix = np.zeros((num_pos, num_pos))
        for pos1 in range(num_pos):
            for pos2 in range(pos1):
                # Aggregate pairwise energies
                # ... extract and average
                pass
        plt.figure(figsize=(10, 8))
        sns.heatmap(pairwise_matrix, annot=True, fmt='.2f', cmap='coolwarm')
        plt.title(f'{title} - Pairwise Energy Matrix')
```

### 3. A* Search Visualization

**What to Visualize**:
- Search tree expansion (if tree structure exposed)
- Node exploration order
- Heuristic vs actual cost
- Search frontier size over time

**Implementation Challenges**:
- A* search tree not currently exposed in bindings
- Would require exposing intermediate search state
- High memory overhead for large searches

**Simplified Approach**:
- Track nodes explored per iteration
- Plot search progress (confs explored vs time)
- Estimate search tree depth from conf indices

### 4. K* Score Comparison Plots

**What to Visualize**:
- C++ vs Java K* scores (scatter plot)
- Relative error distribution
- Computation time comparison
- Convergence comparison

**Implementation**:
```python
def compare_java_cpp(java_results, cpp_results):
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # Scatter plot: Java vs C++
    java_scores = [r.log10_value for r in java_results]
    cpp_scores = [r.log10_value for r in cpp_results]
    
    axes[0, 0].scatter(java_scores, cpp_scores, alpha=0.6)
    axes[0, 0].plot([min(java_scores), max(java_scores)], 
                    [min(java_scores), max(java_scores)], 
                    'r--', label='y=x')
    axes[0, 0].set_xlabel('Java log10(K*)')
    axes[0, 0].set_ylabel('C++ log10(K*)')
    axes[0, 0].set_title('K* Score Comparison')
    axes[0, 0].legend()
    axes[0, 0].grid(True)
    
    # Relative error
    errors = [(c - j) / j * 100 for j, c in zip(java_scores, cpp_scores)]
    axes[0, 1].hist(errors, bins=30, edgecolor='black')
    axes[0, 1].set_xlabel('Relative Error (%)')
    axes[0, 1].set_ylabel('Frequency')
    axes[0, 1].set_title('Relative Error Distribution')
    axes[0, 1].grid(True)
    
    # Timing comparison
    java_times = [r.compute_time for r in java_results]
    cpp_times = [r.compute_time for r in cpp_results]
    axes[1, 0].scatter(java_times, cpp_times, alpha=0.6)
    axes[1, 0].plot([min(java_times), max(java_times)], 
                    [min(java_times), max(java_times)], 
                    'r--', label='y=x')
    axes[1, 0].set_xlabel('Java Time (s)')
    axes[1, 0].set_ylabel('C++ Time (s)')
    axes[1, 0].set_title('Computation Time Comparison')
    axes[1, 0].legend()
    axes[1, 0].grid(True)
```

### 5. Real-Time Progress Visualization

**What to Visualize**:
- Live convergence plots during computation
- Progress bars
- Current state (bounds, delta, confs explored)

**Implementation**:
```python
import matplotlib.pyplot as plt
from matplotlib.animation import FuncAnimation
import threading

class ProgressPlotter:
    def __init__(self):
        self.fig, self.axes = plt.subplots(1, 2, figsize=(14, 5))
        self.lower_history = []
        self.upper_history = []
        self.delta_history = []
        self.num_confs_history = []
        
    def update(self, num_confs, lower, upper, delta):
        self.lower_history.append(lower)
        self.upper_history.append(upper)
        self.delta_history.append(delta)
        self.num_confs_history.append(num_confs)
        
    def plot(self):
        # Update plots (called from animation)
        self.axes[0].clear()
        self.axes[0].plot(self.lower_history, label='Lower')
        self.axes[0].plot(self.upper_history, label='Upper')
        self.axes[0].legend()
        
        self.axes[1].clear()
        self.axes[1].semilogy(self.delta_history)
        plt.draw()
        plt.pause(0.01)

# Usage with callback
plotter = ProgressPlotter()

def progress_callback(num_confs, lower, upper, delta):
    plotter.update(num_confs, lower, upper, delta)
    plotter.plot()

options = kstar_cpp.PartitionFunctionOptions()
options.progress_callback = progress_callback
```

**Libraries**:
- `matplotlib` for plotting
- `seaborn` for enhanced heatmaps
- `plotly` for interactive plots
- `tqdm` for progress bars

### 6. Comprehensive Analysis Dashboard

**What to Visualize**:
- Multi-panel dashboard showing:
  - Energy matrix statistics
  - Partition function convergence (3 panels: protein, ligand, complex)
  - K* score bounds
  - Performance metrics
  - Comparison with Java results

**Implementation**:
```python
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec

def create_analysis_dashboard(protein_emat, ligand_emat, complex_emat,
                              protein_result, ligand_result, complex_result,
                              kstar_result):
    fig = plt.figure(figsize=(16, 12))
    gs = GridSpec(3, 3, figure=fig, hspace=0.3, wspace=0.3)
    
    # Panel 1: Energy distributions
    ax1 = fig.add_subplot(gs[0, 0])
    # ... energy histogram
    
    # Panel 2-4: Convergence plots
    ax2 = fig.add_subplot(gs[0, 1])
    # ... protein convergence
    ax3 = fig.add_subplot(gs[0, 2])
    # ... ligand convergence
    ax4 = fig.add_subplot(gs[1, 0])
    # ... complex convergence
    
    # Panel 5: K* bounds
    ax5 = fig.add_subplot(gs[1, 1])
    # ... K* visualization
    
    # Panel 6: Performance
    ax6 = fig.add_subplot(gs[1, 2])
    # ... timing comparison
    
    plt.savefig('kstar_analysis.png', dpi=300, bbox_inches='tight')
```

## Implementation Priority

### Phase 1 (Immediate)
1. Test with real data files
2. Add basic progress callback (num_confs, bounds, delta)
3. Create convergence plotting script
4. Benchmark against Java

### Phase 2 (Short-term)
1. Energy matrix visualization
2. K* comparison plots
3. Integration with existing Python code
4. Error handling improvements

### Phase 3 (Long-term)
1. Real-time progress visualization
2. Comprehensive analysis dashboard
3. A* search tree visualization (if feasible)
4. Interactive web dashboard (Plotly Dash)

## Required Python Dependencies

```
matplotlib>=3.5.0
numpy>=1.20.0
seaborn>=0.11.0
tqdm>=4.62.0
plotly>=5.0.0  # Optional, for interactive plots
```

## Notes

- Current C++ code has debug output (kPfuncDebug) that could be exposed via callbacks
- Progress tracking requires modification to partition function computation
- Real-time visualization requires threading (compute in background, update plots)
- Energy matrix visualization limited by memory for large matrices
