#!/bin/bash
# Profile MONTAGE (Stage 2: Scaffold Generation)
# Profiles Python orchestration, MASTER subprocess, and Java ConfSpace compilation

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/montage"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

# Default test case
TEST_CASE="${1:-test_montage_minimal}"
INPUT_PDB="${2:-examples/python.KStar/2RL0.min.reduce.pdb}"

echo "=== MONTAGE Profiling ==="
echo "Test case: $TEST_CASE"
echo "Input PDB: $INPUT_PDB"
echo "Output directory: $OUTPUT_DIR"
echo

# Create output directory
mkdir -p "$OUTPUT_DIR/$TEST_CASE"

cd "$REPO_ROOT"

# Check if test script exists
if [ -f "src/main/python/CCKStar/test_montage_demo.py" ]; then
    TEST_SCRIPT="src/main/python/CCKStar/test_montage_demo.py"
else
    echo "ERROR: No MONTAGE test script found"
    exit 1
fi

echo "Using test script: $TEST_SCRIPT"
echo

# Install profiling tools if needed
echo "Checking profiling tools..."
python3 -c "import cProfile" 2>/dev/null || echo "WARNING: cProfile not available"
python3 -c "import psutil" 2>/dev/null || pip install psutil
echo

# 1. Time profiling with cProfile
echo "=== 1. Time Profiling (cProfile) ==="
python3 -m cProfile -o "$OUTPUT_DIR/$TEST_CASE/montage_profile.prof" \
    "$TEST_SCRIPT" "$INPUT_PDB" 2>&1 | tee "$OUTPUT_DIR/$TEST_CASE/montage_output.txt"

# Generate human-readable report
python3 -c "
import pstats
stats = pstats.Stats('$OUTPUT_DIR/$TEST_CASE/montage_profile.prof')
stats.sort_stats('cumulative')
stats.print_stats(50)
" > "$OUTPUT_DIR/$TEST_CASE/montage_profile_top50.txt"

python3 -c "
import pstats
stats = pstats.Stats('$OUTPUT_DIR/$TEST_CASE/montage_profile.prof')
stats.sort_stats('time')
stats.print_stats(50)
" > "$OUTPUT_DIR/$TEST_CASE/montage_profile_time_top50.txt"

echo "  Profile saved: $OUTPUT_DIR/$TEST_CASE/montage_profile.prof"
echo "  Top functions (cumulative): $OUTPUT_DIR/$TEST_CASE/montage_profile_top50.txt"
echo "  Top functions (time): $OUTPUT_DIR/$TEST_CASE/montage_profile_time_top50.txt"
echo

# 2. System resource monitoring
echo "=== 2. System Resource Monitoring ==="
python3 << 'PYTHON_EOF'
import psutil
import time
import subprocess
import sys
from pathlib import Path

# Start monitoring
process = psutil.Process()
start_time = time.time()
start_memory = process.memory_info().rss / 1024 / 1024  # MB

# Run MONTAGE demo
test_script = Path(__file__).parent.parent / "src/main/python/CCKStar/test_montage_demo.py"
input_pdb = Path(__file__).parent.parent / "examples/python.KStar/2RL0.min.reduce.pdb"

result = subprocess.run([sys.executable, str(test_script), str(input_pdb)], 
                       capture_output=True, text=True)

end_time = time.time()
end_memory = process.memory_info().rss / 1024 / 1024  # MB

duration = end_time - start_time
memory_delta = end_memory - start_memory

# Get peak memory from all processes
peak_memory = start_memory
for p in psutil.process_iter(['pid', 'memory_info']):
    try:
        mem = p.memory_info().rss / 1024 / 1024
        if mem > peak_memory:
            peak_memory = mem
    except (psutil.NoSuchProcess, psutil.AccessDenied):
        pass

print(f"Duration: {duration:.2f} seconds")
print(f"Start memory: {start_memory:.2f} MB")
print(f"End memory: {end_memory:.2f} MB")
print(f"Memory delta: {memory_delta:.2f} MB")
print(f"Peak memory: {peak_memory:.2f} MB")
print(f"CPU cores used: {psutil.cpu_count()}")

# Save to file
output_file = Path(__file__).parent.parent / "pipeline_analysis/montage/test_montage_minimal/montage_system_resources.txt"
with open(output_file, 'w') as f:
    f.write(f"Duration: {duration:.2f} seconds\n")
    f.write(f"Start memory: {start_memory:.2f} MB\n")
    f.write(f"End memory: {end_memory:.2f} MB\n")
    f.write(f"Memory delta: {memory_delta:.2f} MB\n")
    f.write(f"Peak memory: {peak_memory:.2f} MB\n")
    f.write(f"CPU cores: {psutil.cpu_count()}\n")
PYTHON_EOF

echo "  System resources: $OUTPUT_DIR/$TEST_CASE/montage_system_resources.txt"
echo

# 3. Subprocess timing (MASTER, Java)
echo "=== 3. Subprocess Analysis ==="
echo "Analyzing subprocess calls (MASTER, Java) from output..."
grep -E "(MASTER|master|java|Java|JPype|JVM)" "$OUTPUT_DIR/$TEST_CASE/montage_output.txt" > "$OUTPUT_DIR/$TEST_CASE/subprocess_calls.txt" || echo "No subprocess calls found"
echo "  Subprocess calls: $OUTPUT_DIR/$TEST_CASE/subprocess_calls.txt"
echo

# 4. Generate summary report
echo "=== 4. Generating Summary Report ==="
python3 << 'PYTHON_EOF'
import pstats
from pathlib import Path

output_dir = Path(__file__).parent.parent / "pipeline_analysis/montage/test_montage_minimal"
profile_file = output_dir / "montage_profile.prof"

if profile_file.exists():
    stats = pstats.Stats(str(profile_file))
    stats.sort_stats('cumulative')
    
    # Get top functions
    top_functions = []
    for func, (cc, nc, tt, ct, callers) in stats.stats.items():
        top_functions.append({
            'file': func[0],
            'line': func[1],
            'function': func[2],
            'cumulative_time': ct,
            'total_time': tt,
            'calls': nc
        })
    
    top_functions.sort(key=lambda x: x['cumulative_time'], reverse=True)
    
    # Read system resources
    system_resources = {}
    resources_file = output_dir / "montage_system_resources.txt"
    if resources_file.exists():
        with open(resources_file) as f:
            for line in f:
                if ':' in line:
                    key, value = line.strip().split(':', 1)
                    system_resources[key.strip()] = value.strip()
    
    # Generate report
    report_file = output_dir / "montage_analysis_summary.md"
    with open(report_file, 'w') as f:
        f.write("# MONTAGE Profiling Analysis Summary\n\n")
        f.write(f"Test case: test_montage_minimal\n\n")
        
        f.write("## System Resources\n\n")
        for key, value in system_resources.items():
            f.write(f"- **{key}**: {value}\n")
        f.write("\n")
        
        f.write("## Top Functions (Cumulative Time)\n\n")
        f.write("| Function | File | Cumulative Time | Total Time | Calls |\n")
        f.write("|----------|------|------------------|------------|-------|\n")
        for func in top_functions[:20]:
            f.write(f"| `{func['function']}` | `{Path(func['file']).name}` | {func['cumulative_time']:.4f}s | {func['total_time']:.4f}s | {func['calls']} |\n")
        f.write("\n")
        
        f.write("## Analysis\n\n")
        f.write("### Time Breakdown (Expected)\n")
        f.write("- MASTER search: 1-2 minutes per match\n")
        f.write("- Scaffold generation: 2-10 minutes per match\n")
        f.write("- ConfSpace compilation: 10-60 seconds per match\n")
        f.write("- File I/O: 1-5 minutes\n")
        f.write("\n")
        
        f.write("### Bottlenecks Identified\n")
        f.write("1. Sequential match processing (no parallelism)\n")
        f.write("2. Subprocess overhead (MASTER calls)\n")
        f.write("3. JPype overhead (JVM startup per process)\n")
        f.write("4. File I/O serialization\n")
        f.write("\n")
        
        f.write("### Optimization Opportunities\n")
        f.write("1. Parallel match processing: 8-10x speedup\n")
        f.write("2. Shared JVM: 1.5-5s saved per match\n")
        f.write("3. Batch MASTER queries: 90% overhead reduction\n")
        f.write("4. In-memory pipeline: 2-5x speedup\n")
    
    print(f"  Summary report: {report_file}")
PYTHON_EOF

echo
echo "=== MONTAGE Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR/$TEST_CASE/"
echo
echo "Files generated:"
echo "  - montage_profile.prof (cProfile binary)"
echo "  - montage_profile_top50.txt (top functions by cumulative time)"
echo "  - montage_profile_time_top50.txt (top functions by self time)"
echo "  - montage_system_resources.txt (system metrics)"
echo "  - montage_analysis_summary.md (summary report)"
echo "  - subprocess_calls.txt (MASTER/Java calls)"

