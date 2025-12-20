# Performance Benchmarking Guide

## Overview

This guide explains how to benchmark performance to compare different branches (e.g., `main` vs `develop` with C++20 modernizations) and verify no performance regressions.

## Quick Start

### Compare Two Separate Repositories (Recommended)

```bash
# Compare separate repository directories (cleaner, no git checkout issues)
./scripts/benchmark_performance.sh ../osprey-fork-main .

# Or with explicit paths
./scripts/benchmark_performance.sh /path/to/osprey-fork-main /path/to/osprey-fork_modern

# Results will be in benchmark_results/ directory
```

### Compare Two Branches (Same Repository)

```bash
# Compare develop vs main branch (requires clean working tree)
./scripts/benchmark_performance.sh --branch main develop

# Results will be in benchmark_results/ directory
```

### Manual Comparison

```bash
# 1. Run tests on main branch (fast subset: 10-20s instead of hours)
git checkout main
./gradlew test \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.assign_native_2RL0_f64" \
  --no-daemon > main-results.txt

# 2. Run tests on develop branch (fast subset)
git checkout develop
./gradlew test \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.assign_native_2RL0_f64" \
  --no-daemon > develop-results.txt

# 3. Extract and compare metrics
python3 scripts/extract_performance_metrics.py main-results.txt develop-results.txt comparison.json comparison.csv
```

## Tools Overview

### 1. `scripts/benchmark_performance.sh`

**Purpose**: Automated performance comparison script

**Usage**:
```bash
# Mode 1: Compare two separate repository directories (RECOMMENDED)
./scripts/benchmark_performance.sh <baseline_dir> <test_dir>

# Mode 2: Compare branches in current repository
./scripts/benchmark_performance.sh --branch <baseline_branch> <test_branch>
```

**Examples**:
```bash
# Compare separate repos (no git checkout needed)
./scripts/benchmark_performance.sh ../osprey-fork-main .

# Compare branches (requires clean working tree)
./scripts/benchmark_performance.sh --branch main develop
```

**What it does**:
- **Directory mode**: Works in separate directories (no git checkout)
- **Branch mode**: Checks out each branch (requires clean working tree)
- Builds C++ library in each location
- Runs `TestNativeConfEnergyCalculator` tests
- Extracts timing metrics
- Generates comparison reports (JSON, CSV, text)

**Output**:
- `benchmark_results/` directory with:
  - Test output files
  - JSON comparison report
  - CSV data (compatible with existing tools)
  - Text summary

### 2. `scripts/extract_performance_metrics.py`

**Purpose**: Extract and compare timing metrics from test output

**Usage**:
```bash
python3 scripts/extract_performance_metrics.py <baseline_file> <test_file> [output_json] [output_csv]
```

**What it does**:
- Parses timing lines from test output (format: `operation: N confs in X.XXs (Y.YY confs/s)`)
- Compares metrics between baseline and test
- Detects performance regressions (>5% slower)
- Outputs JSON, CSV, and human-readable summary

**Output formats**:
- **JSON**: Detailed comparison with all metrics
- **CSV**: Compatible with `scripts/tools/summarize_benchmark_results.py`
- **Console**: Human-readable summary with speedup/regression info

### 3. Integration with Existing Tools

The CSV output is compatible with existing performance analysis tools:

```bash
# Generate statistics summary
python3 scripts/tools/summarize_benchmark_results.py benchmark_results/comparison_TIMESTAMP.csv --versions "main,develop"

# Generate plots (if adapted for branch comparison)
python3 scripts/tools/plot_benchmark_results.py benchmark_results/comparison_TIMESTAMP.csv
```

## Metrics Extracted

The scripts extract timing information from `TestNativeConfEnergyCalculator` test output.

**Note**: The benchmarking script uses a **fast subset** of tests (4 specific test methods) that complete in 10-20 seconds instead of running the full test suite (which takes hours). See `docs/testing/FAST_TESTS_FOR_DEVELOPMENT.md` for details.

- **Operations measured**:
  - `assign`: Coordinate assignment
  - `calcEnergy_all`: Energy calculation (pure energy, no minimization)
  - `minimizeEnergy_all`: Energy minimization (includes CCD minimization overhead)

- **Test methods used**:
  - `calcEnergy_native_all_2RL0_f64` - Best for benchmarking (pure energy calculation)
  - `calcEnergy_native_all_1DG9_6f_f64` - Alternative test case
  - `minimizeEnergy_native_2RL0_f64` - Includes minimization overhead
  - `assign_native_2RL0_f64` - Coordinate assignment

- **Metrics per operation**:
  - Number of conformations processed
  - Total time (seconds)
  - Throughput (conformations/second)

## Performance Regression Detection

The comparison script automatically detects regressions:
- **Threshold**: >5% slower than baseline
- **Action**: Exits with error code 1 if regressions detected
- **Output**: Lists all regressed operations

Example output:
```
⚠️  PERFORMANCE REGRESSIONS DETECTED (>5% slower):
  calcEnergy_all: -7.23% slower
  minimizeEnergy_all: -12.45% slower
```

## GitHub Actions Integration

The `.github/workflows/cpp20-modernization.yml` workflow automatically:
1. Runs tests on both `main` and feature branch
2. Extracts performance metrics
3. Compares and reports differences
4. Fails if regressions detected
5. Uploads comparison artifacts

## Best Practices

### 1. Run Multiple Times

For accurate results, run benchmarks multiple times and average:
```bash
# Run 3 times and compare averages
for i in {1..3}; do
  ./scripts/benchmark_performance.sh main develop
done
```

### 2. Use Consistent Environment

- Same machine/CPU
- Same JVM version
- Minimal background processes
- CPU governor set to performance mode (if possible)

### 3. Focus on Key Operations

The most important operations to monitor:
- `calcEnergy_all`: Core energy calculation
- `minimizeEnergy_all`: Energy minimization (most compute-intensive)

### 4. Check Both Branches

Always verify both branches work correctly:
```bash
# Verify main branch tests pass (fast subset)
git checkout main
./gradlew test \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.assign_native_2RL0_f64"

# Verify develop branch tests pass (fast subset)
git checkout develop
./gradlew test \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.calcEnergy_native_all_1DG9_6f_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.minimizeEnergy_native_2RL0_f64" \
  --tests "edu.duke.cs.osprey.energy.compiled.TestNativeConfEnergyCalculator.assign_native_2RL0_f64"
```

## Example Workflow

### Complete Benchmarking Workflow

**Option A: Using Separate Repositories (Recommended)**

```bash
# 1. Ensure both repositories are up to date
cd ../osprey-fork-main && git pull && cd ../osprey-fork_modern
git pull

# 2. Run comprehensive benchmark
./scripts/benchmark_performance.sh ../osprey-fork-main .

# 3. Review results
cat benchmark_results/comparison_*.txt

# 4. Generate detailed statistics
python3 scripts/tools/summarize_benchmark_results.py \
  benchmark_results/comparison_*.csv \
  --versions "baseline,test"

# 5. Check for regressions
# (Script will exit with error if regressions found)
```

**Option B: Using Branch Comparison**

```bash
# 1. Ensure clean working tree (commit or stash changes)
git status

# 2. Run comprehensive benchmark
./scripts/benchmark_performance.sh --branch main develop

# 3. Review results
cat benchmark_results/comparison_*.txt

# 4. Generate detailed statistics
python3 scripts/tools/summarize_benchmark_results.py \
  benchmark_results/comparison_*.csv \
  --versions "main,develop"

# 5. Check for regressions
# (Script will exit with error if regressions found)
```

## Troubleshooting

### No Timing Metrics Found

**Problem**: Script reports "Found 0 timing metrics"

**Solution**:
- Verify test output contains timing lines (format: `operation: N confs in X.XXs`)
- Check that tests actually ran (look for "PASSED" in output)
- Ensure `TestNativeConfEnergyCalculator` tests executed

### Inconsistent Results

**Problem**: Results vary significantly between runs

**Solution**:
- Run multiple times and average
- Ensure consistent environment (CPU governor, background processes)
- Check for thermal throttling
- Use dedicated benchmarking machine if possible

### CSV Format Issues

**Problem**: CSV not compatible with existing tools

**Solution**:
- Verify CSV has required columns: `version`, `system_size`, `time_us_per_iter`
- Check that `extract_performance_metrics.py` generated CSV correctly
- Use `--versions` flag in `summarize_benchmark_results.py` to match your branch names

## Related Documentation

- `docs/cpp20-modernization/PERFORMANCE_BENCHMARKING_PLAN.md` - Original planning document
- `docs/performance/CPU_OPTIMIZATION_CANDIDATES.md` - Performance optimization opportunities
- `scripts/tools/README.md` - Existing performance tools documentation (if exists)

