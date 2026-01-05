# Python Bindings - Usage Commands

Quick reference for running the Python binding scripts.

## Prerequisites

1. Build the Python module:
```bash
cd $REPO_ROOT
cmake -S src/main/cpp/kstar -B build/cpp/kstar-python \
  -DKSTAR_ENABLE_PYTHON_BINDINGS=ON \
  -DKSTAR_ENABLE_NATIVE_OPT=ON
cmake --build build/cpp/kstar-python -j
```

2. Set PYTHONPATH:
```bash
export PYTHONPATH="$REPO_ROOT/build/cpp/kstar-python:$PYTHONPATH"
```

3. Install Python dependencies (if not already installed):
```bash
pip install matplotlib numpy seaborn  # seaborn is optional
```

## Test the Bindings

Basic import and API test (no data files required):
```bash
cd src/main/cpp/kstar
python test_python_bindings.py
```

## Benchmark Script

### Basic Benchmark
```bash
cd src/main/cpp/kstar
python benchmark_python_vs_java.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99
```

### With Multiple Runs (for averaging)
```bash
python benchmark_python_vs_java.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --runs 5
```

### Compare A* Variants (Baseline vs Fast)
```bash
python benchmark_python_vs_java.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --compare-variants
```

### Epsilon Parameter Sweep
```bash
python benchmark_python_vs_java.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --compare-epsilons
```

### All Comparisons
```bash
python benchmark_python_vs_java.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --runs 3 \
  --compare-variants \
  --compare-epsilons
```

## Visualization Scripts

### Basic Visualization

Simple K* results and energy matrix summary:
```bash
cd src/main/cpp/kstar
python visualize_kstar_results.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --output results
```

This creates:
- `results_kstar.png` - 4-panel K* results plot
- `results_protein_emat.png` - Energy matrix summary

### Advanced Visualization

#### Compare A* Variants
```bash
python visualize_kstar_advanced.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --compare-variants \
  --output advanced
```

#### Epsilon Parameter Sweep
```bash
python visualize_kstar_advanced.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --epsilon-sweep \
  --output advanced
```

#### All Advanced Visualizations
```bash
python visualize_kstar_advanced.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --epsilon 0.99 \
  --compare-variants \
  --epsilon-sweep \
  --output advanced
```

Note: `--heatmap` is available but requires `getOneBody()` binding for full functionality.

### Custom DPI
```bash
python visualize_kstar_results.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --output results \
  --dpi 150
```

## Example Usage Script

Simple Python script example:
```bash
cd src/main/cpp/kstar
python python_bindings_example.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin
```

## Using Real Data Files

If you have test data in the build directory:
```bash
# Navigate to test data directory
cd build/cpp/kstar/test_data

# Run benchmark (adjust file names as needed)
python ../../../../src/main/cpp/kstar/benchmark_python_vs_java.py \
  protein.emat.bin \
  ligand.emat.bin \
  complex.emat.bin \
  --compare-variants \
  --compare-epsilons
```

## Common Options

All scripts support:
- `--epsilon <value>` - Convergence threshold (default: 0.99)
- `--output <prefix>` / `-o <prefix>` - Output file prefix
- `--dpi <value>` - DPI for saved images (default: 75, use 100-150 for better quality)

## Troubleshooting

If you get `ImportError: No module named 'kstar_cpp'`:
```bash
export PYTHONPATH="$PWD/../../../../build/cpp/kstar-python:$PYTHONPATH"
python test_python_bindings.py
```

If files are not found:
```bash
# Check file paths
ls -la protein.emat.bin ligand.emat.bin complex.emat.bin
```

For Windows PowerShell:
```powershell
$env:PYTHONPATH = "$PWD\..\..\..\..\build\cpp\kstar-python;$env:PYTHONPATH"
python test_python_bindings.py
```
