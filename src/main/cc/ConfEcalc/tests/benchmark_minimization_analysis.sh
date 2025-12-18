#!/bin/bash
# Performance analysis script for CCD minimization threading benchmark
# Usage: ./benchmark_minimization_analysis.sh [num_dofs] [iterations] [energy_complexity]

set -e

# Default parameters
NUM_DOFS=${1:-50}
ITERATIONS=${2:-30}
ENERGY_COMPLEXITY=${3:-1000}

BENCHMARK="./benchmark_minimization_ccd"
PERF_AVAILABLE=false

# Check if perf is available
if command -v perf &> /dev/null; then
    PERF_AVAILABLE=true
fi

# Check if benchmark exists
if [ ! -f "$BENCHMARK" ]; then
    echo "Error: Benchmark not found at $BENCHMARK"
    echo "Please build it first: cd build && make benchmark_minimization_ccd"
    exit 1
fi

echo "=== CCD Minimization Threading Performance Analysis ==="
echo "Parameters: DOFs=$NUM_DOFS, Iterations=$ITERATIONS, Energy Complexity=$ENERGY_COMPLEXITY"
echo ""

# Run basic benchmark
echo "1. Running basic benchmark..."
$BENCHMARK $NUM_DOFS $ITERATIONS $ENERGY_COMPLEXITY

echo ""
echo "---"

# Run with perf if available
if [ "$PERF_AVAILABLE" = true ]; then
    echo ""
    echo "2. Running with perf profiler (sequential)..."
    echo "   Top 20 functions by time:"
    perf record -g --call-graph dwarf -o perf_seq.data -- $BENCHMARK $NUM_DOFS $ITERATIONS $ENERGY_COMPLEXITY > /dev/null 2>&1
    perf report --stdio -i perf_seq.data | head -40
    
    echo ""
    echo "3. Performance statistics (sequential):"
    perf stat -d -- $BENCHMARK $NUM_DOFS $ITERATIONS $ENERGY_COMPLEXITY 2>&1 | grep -E "(task-clock|context-switches|cpu-migrations|page-faults|cache-misses)"
    
    echo ""
    echo "Perf data saved to: perf_seq.data"
    echo "View with: perf report -i perf_seq.data"
else
    echo ""
    echo "2. perf not available - skipping detailed profiling"
    echo "   Install perf for detailed performance analysis:"
    echo "   - Ubuntu/Debian: sudo apt-get install linux-perf"
    echo "   - RHEL/CentOS: sudo yum install perf"
fi

echo ""
echo "=== Analysis Complete ==="
echo ""
echo "Recommendations:"
echo "- If speedup < 1.0: Threading overhead too high, disable threading"
echo "- If speedup 1.0-1.1: Threading provides minimal benefit, consider disabling"
echo "- If speedup > 1.2: Threading beneficial, enable by default"
echo "- Check critical section overhead - if > 50%, consider per-thread copies"

