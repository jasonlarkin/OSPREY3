#!/bin/bash
# Profile SIMD implementations with perf to identify bottlenecks
# Usage: ./profile_simd.sh [workload_size]
#   workload_size: number of iterations (default: 5000)

set -e

WORKLOAD=${1:-5000}
BENCHMARK_BIN="./build/benchmark_simd_direct"
OUTPUT_DIR="perf_results"
TIMESTAMP=$(date +%Y%m%d_%H%M%S)

echo "=== SIMD Performance Profiling ==="
echo "Workload: $WORKLOAD iterations"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Create output directory
mkdir -p "$OUTPUT_DIR"

# Function to profile a specific version
profile_version() {
    local version=$1
    local env_var=$2
    local output_file="$OUTPUT_DIR/perf_${version}_${TIMESTAMP}.data"
    local report_file="$OUTPUT_DIR/perf_${version}_${TIMESTAMP}.txt"
    
    echo "Profiling $version..."
    
    # Set environment variable if provided
    if [ -n "$env_var" ]; then
        export $env_var
    else
        unset OSPREY_FORCE_SCALAR
        unset OSPREY_USE_FAST_EXP
    fi
    
    # Record with perf (capture stderr for errors)
    perf record -g -o "$output_file" -- $BENCHMARK_BIN $WORKLOAD 2>&1 | grep -v "WARNING: Kernel address maps" || true
    
    # Generate report
    perf report -i "$output_file" --stdio > "$report_file" 2>&1 || echo "Warning: Could not generate report for $version"
    
    # Generate annotated assembly (if available)
    perf annotate -i "$output_file" > "$OUTPUT_DIR/perf_${version}_${TIMESTAMP}_annotate.txt" 2>&1 || true
    
    # Generate call graph
    perf report -i "$output_file" --call-graph --stdio > "$OUTPUT_DIR/perf_${version}_${TIMESTAMP}_callgraph.txt" 2>&1 || true
    
    echo "  Results saved to: $output_file"
    echo "  Report saved to: $report_file"
    echo ""
    
    # Unset environment variable
    unset OSPREY_FORCE_SCALAR
    unset OSPREY_USE_FAST_EXP
}

# Profile all versions
echo "=== Profiling Scalar (baseline) ==="
profile_version "scalar" "OSPREY_FORCE_SCALAR=1"

echo "=== Profiling AVX2 (exact exp) ==="
profile_version "avx2_exact" ""

echo "=== Profiling AVX-512 (exact exp) ==="
profile_version "avx512_exact" ""

# Generate summary
echo "=== Generating Summary ==="
cat > "$OUTPUT_DIR/summary_${TIMESTAMP}.txt" << EOF
SIMD Performance Profiling Summary
Generated: $(date)
Workload: $WORKLOAD iterations

Files generated:
- perf_scalar_${TIMESTAMP}.data: Scalar profile data
- perf_scalar_${TIMESTAMP}.txt: Scalar report
- perf_avx2_exact_${TIMESTAMP}.data: AVX2 profile data
- perf_avx2_exact_${TIMESTAMP}.txt: AVX2 report
- perf_avx512_exact_${TIMESTAMP}.data: AVX-512 profile data
- perf_avx512_exact_${TIMESTAMP}.txt: AVX-512 report

To view reports:
  perf report -i perf_scalar_${TIMESTAMP}.data
  perf report -i perf_avx2_exact_${TIMESTAMP}.data
  perf report -i perf_avx512_exact_${TIMESTAMP}.data

To compare:
  perf diff perf_scalar_${TIMESTAMP}.data perf_avx2_exact_${TIMESTAMP}.data
  perf diff perf_avx2_exact_${TIMESTAMP}.data perf_avx512_exact_${TIMESTAMP}.data
EOF

echo "Summary saved to: $OUTPUT_DIR/summary_${TIMESTAMP}.txt"
echo ""
echo "=== Profiling Complete ==="
echo "Results in: $OUTPUT_DIR/"

