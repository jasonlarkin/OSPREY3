# Pipeline Analysis Tools

Tools for analyzing OSPREY pipeline to understand memory patterns, data structures, and multiprocessing opportunities.

## Quick Start

```bash
# Run comprehensive analysis
./scripts/run_pipeline_analysis.sh

# Or run individual tools
python3 scripts/visualize_data_structures.py --output-dir visualizations
python3 scripts/analyze_multiprocessing.py --output-dir multiprocessing_analysis
```

## Tools

### 1. Memory Analysis (`analyze_pipeline_memory.py`)

Analyzes memory allocation patterns using perf, valgrind, or JVM profiling.

**Usage:**
```bash
# With valgrind (recommended for detailed analysis)
python3 scripts/analyze_pipeline_memory.py \
    --command "./gradlew test --tests TestKStar" \
    --stage kstar \
    --tool valgrind \
    --output-dir memory_analysis

# With perf (faster, less detailed)
python3 scripts/analyze_pipeline_memory.py \
    --command "./gradlew test --tests TestKStar" \
    --tool perf \
    --output-dir memory_analysis

# JVM profiling (for Java/K* analysis)
python3 scripts/analyze_pipeline_memory.py \
    --java-pid $(pgrep -f "TestKStar") \
    --tool jvm \
    --output-dir memory_analysis
```

**Outputs:**
- `memory_timeline.png` - Memory usage over time
- `allocation_opportunities.md` - Identified allocation-churn reduction opportunities (if generated)
- `kstar_memory_analysis.json` - Detailed memory analysis

### 2. Data Structure Visualization (`visualize_data_structures.py`)

Creates visualizations of data structure lifetimes and resource-lifetime boundaries.

**Usage:**
```bash
# With trace file (if available)
python3 scripts/visualize_data_structures.py \
    --trace-file kstar_trace.csv \
    --output-dir visualizations

# Without trace (uses example data)
python3 scripts/visualize_data_structures.py \
    --output-dir visualizations
```

**Outputs:**
- `lifetime_timeline.png` - Data structure lifetimes
- `allocation_boundaries.png` - Resource-lifetime boundaries (if generated)
- `memory_fragmentation.png` - Memory fragmentation patterns

### 3. Multiprocessing Analysis (`analyze_multiprocessing.py`)

Analyzes MPI and threading opportunities.

**Usage:**
```bash
# With custom config
python3 scripts/analyze_multiprocessing.py \
    --config pipeline_config.json \
    --output-dir multiprocessing_analysis

# With default config
python3 scripts/analyze_multiprocessing.py \
    --output-dir multiprocessing_analysis
```

**Outputs:**
- `mpi_architecture.png` - MPI architecture diagram
- `mpi_config.json` - MPI configuration
- `analysis.json` - Detailed parallelism analysis

### 4. SLURM Setup (`setup_slurm_local.sh`)

Sets up SLURM locally for testing (WSL/Linux).

**Usage:**
```bash
# Install and configure SLURM
./scripts/setup_slurm_local.sh

# Test SLURM
sinfo
srun hostname
sbatch --wrap='sleep 10'
```

## Dependencies

### Python Packages
```bash
# Activate optional tooling venv (set this to your local venv path)
export OSPREY_TOOLS_VENV=/path/to/venv
source "$OSPREY_TOOLS_VENV/bin/activate"

# Install if needed
pip install matplotlib numpy
```

### System Tools
```bash
# Ubuntu/Debian
sudo apt-get install valgrind linux-perf python3-matplotlib python3-numpy

# For SLURM
sudo apt-get install slurm-wlm slurm-wlm-doc
```

## Example Workflow

1. **Profile K* algorithm memory:**
   ```bash
   python3 scripts/analyze_pipeline_memory.py \
       --command "./gradlew test --tests TestKStar" \
       --stage kstar \
       --tool valgrind \
       --output-dir kstar_memory
   ```

2. **Visualize data structures:**
   ```bash
   python3 scripts/visualize_data_structures.py \
       --output-dir visualizations
   ```

3. **Analyze multiprocessing:**
   ```bash
   python3 scripts/analyze_multiprocessing.py \
       --output-dir multiprocessing
   ```

4. **Review results:**
   - Check `kstar_memory/allocation_opportunities.md` for allocation-churn reduction notes
   - Review `visualizations/allocation_boundaries.png` for resource-lifetime boundaries
   - Check `multiprocessing/mpi_architecture.png` for MPI design

## Integration with Existing Tools

These tools integrate with existing OSPREY analysis tools:
- `scripts/tools/plot_benchmark_results.py` - Benchmark visualization
- `scripts/extract_performance_metrics.py` - Performance metrics
- `scripts/benchmark_performance.sh` - Performance benchmarking

## WSL Notes

All scripts are designed for WSL/Linux. Paths are automatically detected:
- Project root: Detected from script location
- Venv: Uses `OSPREY_TOOLS_VENV` when set; otherwise searches common repo-relative locations (see `scripts/lib/osprey_env.sh`)
- Outputs: Created in project-relative directories

