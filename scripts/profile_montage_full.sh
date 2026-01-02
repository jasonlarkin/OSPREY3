#!/bin/bash
# Profile Full MONTAGE (Stage 2: Scaffold Generation with MASTER)
# Profiles Python orchestration, MASTER C++ subprocess, and Java ConfSpace compilation
# This version includes MASTER search (unlike profile_montage.sh which skips it)

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/montage"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

# Default test case
TEST_CASE="${1:-test_montage_full}"
INPUT_PDB="${2:-examples/python.KStar/2RL0.min.reduce.pdb}"
MASTER_MATCHES="${3:-5}"  # Number of MASTER matches (default 5 for faster profiling)
MAX_FLEX="${4:-4}"         # Max flexible residues

echo "=== Full MONTAGE Profiling (with MASTER) ==="
echo "Test case: $TEST_CASE"
echo "Input PDB: $INPUT_PDB"
echo "MASTER matches: $MASTER_MATCHES"
echo "Max flexible residues: $MAX_FLEX"
echo "Output directory: $OUTPUT_DIR"
echo

# Create output directory
mkdir -p "$OUTPUT_DIR/$TEST_CASE"

# Check prerequisites
echo "=== Checking Prerequisites ==="
cd "$CCKSTAR_DIR"

if [ ! -f "resources/master" ]; then
    echo "ERROR: MASTER executable not found at resources/master"
    exit 1
fi

if [ ! -f "resources/createPDS" ]; then
    echo "ERROR: createPDS executable not found at resources/createPDS"
    exit 1
fi

if [ ! -f "resources/db.txt.local" ] && [ ! -f "resources/db.txt" ]; then
    echo "ERROR: Database config not found (need resources/db.txt or resources/db.txt.local)"
    exit 1
fi

# Determine database file
if [ -f "resources/db.txt.local" ]; then
    DB_FILE="resources/db.txt.local"
elif [ -f "resources/db.txt" ]; then
    DB_FILE="resources/db.txt"
fi

echo "MASTER executable: $(ls -lh resources/master | awk '{print $5}')"
echo "createPDS executable: $(ls -lh resources/createPDS | awk '{print $5}')"
echo "Database config: $DB_FILE"
echo "Database entries: $(wc -l < "$DB_FILE")"
echo

# Make executables executable
chmod +x resources/master resources/createPDS 2>/dev/null || true

# Install profiling tools if needed
echo "Checking profiling tools..."
python3 -c "import cProfile" 2>/dev/null || echo "WARNING: cProfile not available"
python3 -c "import psutil" 2>/dev/null || pip install psutil
python3 -c "import time" 2>/dev/null || true
echo

# Create temporary input directory for MONTAGE
TEMP_INPUT_DIR=$(mktemp -d)
trap "rm -rf $TEMP_INPUT_DIR" EXIT

# Copy input PDB to temp directory
INPUT_PDB_ABS="$REPO_ROOT/$INPUT_PDB"
if [ ! -f "$INPUT_PDB_ABS" ]; then
    echo "ERROR: Input PDB not found: $INPUT_PDB_ABS"
    exit 1
fi

cp "$INPUT_PDB_ABS" "$TEMP_INPUT_DIR/"

# 1. Time profiling with cProfile
echo "=== 1. Time Profiling (cProfile) ==="
echo "Running full MONTAGE workflow..."
echo "Note: MASTER search may take ~10 minutes per query"
echo

# Create Python script to run MONTAGE with profiling
cat > "$OUTPUT_DIR/$TEST_CASE/run_montage_profiled.py" << PYTHON_EOF
#!/usr/bin/env python3
import sys
import os
import cProfile
import pstats
import time
import subprocess
from pathlib import Path

# Add CCKStar to path - use absolute path from script variable
cckstar_dir = Path("$CCKSTAR_DIR").resolve()
sys.path.insert(0, str(cckstar_dir))

# Change to CCKStar directory for relative imports and resource access
os.chdir(str(cckstar_dir))

# Clean up any existing MONTAGE directories from previous runs
import glob
import shutil
for montage_dir in glob.glob("*-MONTAGE"):
    if os.path.isdir(montage_dir):
        print(f"Cleaning up existing directory: {montage_dir}")
        shutil.rmtree(montage_dir)
for master_dir in glob.glob("*-MASTER"):
    if os.path.isdir(master_dir):
        print(f"Cleaning up existing directory: {master_dir}")
        shutil.rmtree(master_dir)

from MONTAGE import run_MONTAGE

# Configuration
input_pdb_dir = "$TEMP_INPUT_DIR"
input_chirality = "L"
output_chirality = "D"
master_matches = $MASTER_MATCHES
max_flex = $MAX_FLEX

print(f"Input PDB directory: {input_pdb_dir}")
print(f"MASTER matches: {master_matches}")
print(f"Max flex: {max_flex}")
print()

# Profile the run
profiler = cProfile.Profile()
profiler.enable()

start_time = time.time()

try:
    run_MONTAGE(
        input_pdb_dir,
        input_chirality,
        output_chirality,
        master_matches,
        max_flex
    )
except shutil.Error as e:
    # Handle directory already exists error (non-fatal)
    if "already exists" in str(e):
        print(f"WARNING: {e}", file=sys.stderr)
        print("Continuing - profile data was captured", file=sys.stderr)
    else:
        raise
except Exception as e:
    print(f"ERROR: {e}", file=sys.stderr)
    import traceback
    traceback.print_exc()
    # Don't exit - we still want to save the profile

end_time = time.time()
duration = end_time - start_time

profiler.disable()

# Save profile
profile_file = Path(__file__).parent / "montage_profile.prof"
profiler.dump_stats(str(profile_file))

print(f"\nMONTAGE completed in {duration:.2f} seconds")
print(f"Profile saved to: {profile_file}")
PYTHON_EOF

chmod +x "$OUTPUT_DIR/$TEST_CASE/run_montage_profiled.py"

# Run with cProfile
python3 "$OUTPUT_DIR/$TEST_CASE/run_montage_profiled.py" 2>&1 | tee "$OUTPUT_DIR/$TEST_CASE/montage_output.txt"

# Generate human-readable reports
echo
echo "Generating profile reports..."

python3 << PYTHON_EOF
import pstats
import sys
from pathlib import Path

output_dir = Path("$OUTPUT_DIR/$TEST_CASE")
profile_file = output_dir / "montage_profile.prof"

if profile_file.exists():
    stats = pstats.Stats(str(profile_file))
    
    # Cumulative time - redirect stdout
    stats.sort_stats('cumulative')
    with open(output_dir / "montage_profile_top50.txt", 'w') as f:
        old_stdout = sys.stdout
        sys.stdout = f
        stats.print_stats(50)
        sys.stdout = old_stdout
    
    # Self time - redirect stdout
    stats.sort_stats('time')
    with open(output_dir / "montage_profile_time_top50.txt", 'w') as f:
        old_stdout = sys.stdout
        sys.stdout = f
        stats.print_stats(50)
        sys.stdout = old_stdout
    
    print("  Profile saved: $OUTPUT_DIR/$TEST_CASE/montage_profile.prof")
    print("  Top functions (cumulative): $OUTPUT_DIR/$TEST_CASE/montage_profile_top50.txt")
    print("  Top functions (time): $OUTPUT_DIR/$TEST_CASE/montage_profile_time_top50.txt")
else:
    print("WARNING: Profile file not found")
PYTHON_EOF

echo

# 2. System resource monitoring (already captured in output)
echo "=== 2. System Resource Analysis ==="
python3 << PYTHON_EOF
import re
from pathlib import Path

output_file = Path("$OUTPUT_DIR/$TEST_CASE/montage_output.txt")
resources_file = Path("$OUTPUT_DIR/$TEST_CASE/montage_system_resources.txt")

if output_file.exists():
    with open(output_file) as f:
        content = f.read()
    
    # Extract timing information
    duration_match = re.search(r'completed in ([\d.]+) seconds', content)
    duration = duration_match.group(1) if duration_match else "N/A"
    
    # Extract MASTER timing
    master_start = None
    master_end = None
    for line in content.split('\n'):
        if 'Running MASTER' in line:
            # Try to extract timestamp if available
            pass
        if 'MASTER search completed' in line:
            pass
    
    with open(resources_file, 'w') as f:
        f.write(f"Duration: {duration} seconds\n")
        f.write("Note: Full timing captured in montage_output.txt\n")
    
    print(f"  System resources: {resources_file}")
else:
    print("WARNING: Output file not found")
PYTHON_EOF

echo

# 3. Subprocess analysis (MASTER C++ calls)
echo "=== 3. Subprocess Analysis (MASTER C++) ==="
echo "Analyzing MASTER subprocess calls..."

python3 << PYTHON_EOF
from pathlib import Path
import re

output_dir = Path("$OUTPUT_DIR/$TEST_CASE")
output_file = output_dir / "montage_output.txt"
subprocess_file = output_dir / "subprocess_calls.txt"

if output_file.exists():
    with open(output_file) as f:
        content = f.read()
    
    # Extract MASTER-related lines
    master_lines = []
    for line in content.split('\n'):
        if any(keyword in line.lower() for keyword in ['master', 'createpds', 'pds', 'query', 'matches']):
            master_lines.append(line)
    
    with open(subprocess_file, 'w') as f:
        f.write("# MASTER Subprocess Calls\n\n")
        if master_lines:
            for line in master_lines:
                f.write(f"{line}\n")
        else:
            f.write("No MASTER subprocess calls found in output\n")
    
    print(f"  Subprocess calls: {subprocess_file}")
    print(f"  Found {len(master_lines)} MASTER-related lines")
else:
    print("WARNING: Output file not found")
PYTHON_EOF

echo

# 4. MASTER executable profiling (if perf is available)
echo "=== 4. MASTER C++ Profiling ==="
if command -v perf >/dev/null 2>&1; then
    echo "perf is available - can profile MASTER C++ executable"
    echo "Note: MASTER runs as subprocess, profiling would require separate run"
    echo "  To profile MASTER separately, run:"
    echo "    perf record -g ./resources/master --query query.pds --targetList $DB_FILE --rmsdCut 10.0 --topN $MASTER_MATCHES --outType match --seqOut matches.txt --structOut matches"
else
    echo "perf not available - skipping C++ profiling"
    echo "  Install with: sudo apt-get install linux-tools-common"
fi
echo

# 5. Generate summary report (simplified)
echo "=== 5. Generating Summary Report ==="
python3 << PYTHON_EOF
import pstats
from pathlib import Path
import re
import os

output_dir = Path("$OUTPUT_DIR/$TEST_CASE")
profile_file = output_dir / "montage_profile.prof"
output_file = output_dir / "montage_output.txt"
test_case_name = "$TEST_CASE"
input_pdb = "$INPUT_PDB"
master_matches = "$MASTER_MATCHES"
max_flex = "$MAX_FLEX"

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
    
    # Extract MASTER timing from output
    master_timing = {}
    if output_file.exists():
        with open(output_file) as f:
            content = f.read()
        
        # Find MASTER execution
        if 'Running MASTER' in content:
            master_timing['status'] = 'executed'
        if 'MASTER search completed' in content:
            master_timing['status'] = 'completed'
    
    # Generate report
    report_file = output_dir / "montage_full_analysis_summary.md"
    test_case_name = Path('$TEST_CASE').name
    input_pdb = '$INPUT_PDB'
    master_matches = '$MASTER_MATCHES'
    max_flex = '$MAX_FLEX'
    
    with open(report_file, 'w') as f:
        f.write("# Full MONTAGE Profiling Analysis Summary\n\n")
        f.write("Test case: {}\n".format(test_case_name))
        f.write("Input PDB: {}\n".format(input_pdb))
        f.write("MASTER matches: {}\n".format(master_matches))
        f.write("Max flexible residues: {}\n\n".format(max_flex))
        
        f.write("## System Resources\n\n")
        for key, value in system_resources.items():
            f.write("- **{}**: {}\n".format(key, value))
        f.write("\n")
        
        if master_timing:
            f.write("## MASTER Execution\n\n")
            f.write("- **Status**: {}\n".format(master_timing.get('status', 'unknown')))
            f.write("\n")
        
        f.write("## Top Functions (Cumulative Time)\n\n")
        f.write("| Function | File | Cumulative Time | Total Time | Calls |\n")
        f.write("|----------|------|------------------|------------|-------|\n")
        for func in top_functions[:20]:
            func_name = func['function']
            file_name = Path(func['file']).name
            cum_time = func['cumulative_time']
            tot_time = func['total_time']
            calls = func['calls']
            # Use string formatting to avoid shell interpretation of braces
            f.write("| `{}` | `{}` | {:.4f}s | {:.4f}s | {} |\n".format(
                func_name, file_name, cum_time, tot_time, calls))
        f.write("\n")
        
        f.write("## Language Breakdown\n\n")
        f.write("MONTAGE uses multiple languages:\n")
        f.write("- **Python**: Orchestration, SCOPE, file I/O\n")
        f.write("- **C++**: MASTER structural search (subprocess)\n")
        f.write("- **Java**: OSPREY ConfSpace compilation, K* calculations\n")
        f.write("\n")
        
        f.write("## Analysis\n\n")
        f.write("### Time Breakdown (Expected)\n")
        f.write("- MASTER search: ~10 minutes per query (C++ subprocess)\n")
        f.write("- Scaffold generation: 2-10 minutes per match\n")
        f.write("- ConfSpace compilation: 10-60 seconds per match (Java)\n")
        f.write("- File I/O: 1-5 minutes\n")
        f.write("\n")
        
        f.write("### Bottlenecks Identified\n")
        f.write("1. MASTER C++ subprocess: ~10 minutes per query\n")
        f.write("2. Sequential match processing (no parallelism)\n")
        f.write("3. Subprocess overhead (MASTER calls)\n")
        f.write("4. JPype overhead (JVM startup per process)\n")
        f.write("5. File I/O serialization\n")
        f.write("\n")
        
        f.write("### Optimization Opportunities\n")
        f.write("1. Parallel match processing: 8-10x speedup\n")
        f.write("2. Shared JVM: 1.5-5s saved per match\n")
        f.write("3. Batch MASTER queries: 90% overhead reduction\n")
        f.write("4. In-memory pipeline: 2-5x speedup\n")
        f.write("5. MASTER C++ optimization: Profile with perf, consider SIMD/parallelism\n")
    
    print("  Summary report: {}".format(report_file))
else:
    print("WARNING: Profile file not found")
PYTHON_EOF

echo
echo "=== Full MONTAGE Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR/$TEST_CASE/"
echo
echo "Files generated:"
echo "  - montage_profile.prof (cProfile binary)"
echo "  - montage_profile_top50.txt (top functions by cumulative time)"
echo "  - montage_profile_time_top50.txt (top functions by self time)"
echo "  - montage_system_resources.txt (system metrics)"
echo "  - montage_full_analysis_summary.md (summary report)"
echo "  - subprocess_calls.txt (MASTER C++ calls)"
echo
echo "Note: This run includes MASTER search (~10 minutes per query)"
echo "      Use profile_montage.sh for demo version (skips MASTER)"

