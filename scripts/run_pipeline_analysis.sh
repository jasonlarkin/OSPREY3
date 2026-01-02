#!/bin/bash
# Comprehensive pipeline analysis script
# Runs all analysis tools and generates visualizations

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/pipeline_analysis"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$PROJECT_ROOT/scripts/lib/osprey_env.sh"
VENV_PATH="$(osprey_find_tools_venv "$PROJECT_ROOT")"

echo "=== OSPREY Pipeline Analysis ==="
echo "Project root: $PROJECT_ROOT"
echo "Output directory: $OUTPUT_DIR"

# Activate venv if available
if [ -n "$VENV_PATH" ] && [ -f "$VENV_PATH/bin/activate" ]; then
    echo "Activating Python venv: $VENV_PATH"
    # shellcheck disable=SC1090
    source "$VENV_PATH/bin/activate"
    echo "Python: $(which python3)"
    echo "Python version: $(python3 --version)"
else
    echo "Warning: venv not found"
    echo "Tip: set OSPREY_TOOLS_VENV=/path/to/venv to enable optional analysis tooling"
    echo "Using system Python: $(which python3)"
fi

# Create output directories
mkdir -p "$OUTPUT_DIR"/{memory,visualizations,multiprocessing,reports}

echo ""
echo "=== Step 1: Memory Analysis ==="
echo "Profiling actual OSPREY execution..."

# Check if we should run actual profiling
if [ "${SKIP_PROFILING:-}" != "1" ]; then
    echo "Running K* profiling (this may take several minutes)..."
    echo "Set SKIP_PROFILING=1 to skip actual execution profiling"
    echo ""
    
    # Run the profiling script
    if [ -f "$SCRIPT_DIR/profile_kstar_execution.sh" ]; then
        bash "$SCRIPT_DIR/profile_kstar_execution.sh"
    else
        echo "Warning: profile_kstar_execution.sh not found"
        echo "Skipping actual execution profiling"
    fi
else
    echo "Skipping actual execution profiling (SKIP_PROFILING=1)"
fi

# Analyze any existing profiling data
if [ -f "$OUTPUT_DIR/memory/kstar_java_massif.out" ] || [ -f "$OUTPUT_DIR/memory/kstar_python_massif.out" ]; then
    echo ""
    echo "Analyzing existing profiling data..."
    
    # Find massif file
    MASSIF_FILE=""
    if [ -f "$OUTPUT_DIR/memory/kstar_java_massif.out" ]; then
        MASSIF_FILE="$OUTPUT_DIR/memory/kstar_java_massif.out"
    elif [ -f "$OUTPUT_DIR/memory/kstar_python_massif.out" ]; then
        MASSIF_FILE="$OUTPUT_DIR/memory/kstar_python_massif.out"
    fi
    
    if [ -n "$MASSIF_FILE" ]; then
        python3 "$SCRIPT_DIR/analyze_pipeline_memory.py" \
            --stage kstar \
            --output-dir "$OUTPUT_DIR/memory" \
            --massif-file "$MASSIF_FILE"
    fi
else
    echo ""
    echo "No profiling data found. To generate profiling data:"
    echo "  bash $SCRIPT_DIR/profile_kstar_execution.sh"
    echo ""
    echo "Or run manually:"
    echo "  python3 $SCRIPT_DIR/analyze_pipeline_memory.py \\"
    echo "      --command './gradlew test --tests TestKStar' \\"
    echo "      --stage kstar \\"
    echo "      --tool valgrind \\"
    echo "      --output-dir $OUTPUT_DIR/memory"
fi

echo ""
echo "=== Step 2: Data Structure Visualization ==="
echo "Generating data structure lifetime visualizations..."

python3 "$SCRIPT_DIR/visualize_data_structures.py" \
    --output-dir "$OUTPUT_DIR/visualizations"

echo ""
echo "=== Step 3: Multiprocessing Analysis ==="
echo "Analyzing MPI and threading opportunities..."

python3 "$SCRIPT_DIR/analyze_multiprocessing.py" \
    --output-dir "$OUTPUT_DIR/multiprocessing"

echo ""
echo "=== Step 4: Generate Summary Report ==="
echo "Creating analysis summary..."

cat > "$OUTPUT_DIR/reports/analysis_summary.md" <<EOF
# OSPREY Pipeline Analysis Summary

Generated: $(date)

## Analysis Components

1. **Memory Analysis**: $OUTPUT_DIR/memory/
   - Memory allocation patterns
   - GC pressure analysis
   - Allocation-churn notes (if present)

2. **Data Structure Visualization**: $OUTPUT_DIR/visualizations/
   - Lifetime timelines
   - Resource-lifetime boundaries (if present)
   - Memory fragmentation

3. **Multiprocessing Analysis**: $OUTPUT_DIR/multiprocessing/
   - MPI architecture design
   - Threading opportunities
   - Shared memory requirements

## Next Steps

  1. Review visualizations in $OUTPUT_DIR/visualizations/
  2. Review memory notes in $OUTPUT_DIR/memory/ (if present)
  3. Review multiprocessing notes in $OUTPUT_DIR/multiprocessing/
  4. Iterate profiling runs and update writeups

EOF

echo ""
echo "=== Analysis Complete ==="
echo "Results saved to: $OUTPUT_DIR"
echo ""
echo "Key files:"
echo "  - Visualizations: $OUTPUT_DIR/visualizations/"
echo "  - Memory analysis: $OUTPUT_DIR/memory/"
echo "  - MPI architecture: $OUTPUT_DIR/multiprocessing/"
echo "  - Summary: $OUTPUT_DIR/reports/analysis_summary.md"

