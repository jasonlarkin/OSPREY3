#!/bin/bash
# Profile parallel ConfSpace compilation
# Tests parallel compilation of multiple confspaces

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/confspace_compilation"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"

# Optional external tools venv (portable; no hardcoded absolute paths)
# shellcheck disable=SC1091
source "$REPO_ROOT/scripts/lib/osprey_env.sh"
osprey_try_activate_tools_venv "$REPO_ROOT" || true

# Defaults
CONFSPACE_FILES="${@}"
TEST_NAME="${TEST_NAME:-test_parallel}"

if [ -z "$CONFSPACE_FILES" ]; then
    echo "Usage: $0 <confspace1> [confspace2] [confspace3] ..."
    echo ""
    echo "Example:"
    echo "  $0 target.confspace design.confspace complex.confspace"
    echo ""
    echo "Or set CONFSPACE_FILES environment variable:"
    echo "  CONFSPACE_FILES='target.confspace design.confspace complex.confspace' $0"
    exit 1
fi

echo "=== Parallel ConfSpace Compilation Profiling ==="
echo "ConfSpace files: $CONFSPACE_FILES"
echo "Test name: $TEST_NAME"
echo "Output: $OUTPUT_DIR/$TEST_NAME"
echo ""

mkdir -p "$OUTPUT_DIR/$TEST_NAME"

# Create parallel profiling script
cat > "$OUTPUT_DIR/$TEST_NAME/profile_parallel.py" << 'PYEOF'
import sys
import os
import time
import cProfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

confspace_files = sys.argv[1:-2]
output_dir = Path(sys.argv[-2])
test_name = sys.argv[-1]

sys.path.insert(0, str(Path(os.environ.get('CCKSTAR_DIR', '.')).resolve()))
import jpype
if not jpype.isJVMStarted():
    import osprey
    osprey.start()
import osprey.prep

def compile_single(confspace_file):
    """Compile a single confspace"""
    start = time.time()
    result = {
        'file': confspace_file,
        'start_time': start,
        'load_time': 0,
        'compile_time': 0,
        'save_time': 0,
        'total_time': 0,
        'error': None
    }
    
    try:
        # Load
        load_start = time.time()
        with open(confspace_file, 'r') as f:
            confspace_content = f.read()
        confspace = osprey.prep.loadConfSpace(confspace_content)
        result['load_time'] = time.time() - load_start
        
        # Compile (need LocalService for AmberTools)
        compile_start = time.time()
        compiler = osprey.prep.ConfSpaceCompiler(confspace)
        compiler.getForcefields().add(osprey.prep.Forcefield.Amber96)
        compiler.getForcefields().add(osprey.prep.Forcefield.EEF1)
        
        with osprey.prep.LocalService():
            progress = compiler.compile()
            progress.printUntilFinish(10000)
            report = progress.getReport()
        
        if report.getError() is not None:
            raise Exception('Compilation failed', report.getError())
        
        result['compile_time'] = time.time() - compile_start
        
        # Save
        save_start = time.time()
        save_path = confspace_file.replace('.confspace', '.ccsx')
        compiled_bytes = osprey.prep.saveCompiledConfSpace(report.getCompiled())
        with open(save_path, 'wb') as f:
            f.write(compiled_bytes)
        result['save_time'] = time.time() - save_start
        
        result['total_time'] = time.time() - start
        
    except Exception as e:
        result['error'] = str(e)
        result['total_time'] = time.time() - start
    
    return result

# Profile parallel execution
profiler = cProfile.Profile()
profiler.enable()

start_time = time.time()

# Sequential compilation
print("=== Sequential Compilation ===")
sequential_results = []
for confspace_file in confspace_files:
    print(f"Compiling {confspace_file}...")
    result = compile_single(confspace_file)
    sequential_results.append(result)
    if result['error']:
        print(f"  ERROR: {result['error']}")
    else:
        print(f"  Time: {result['total_time']:.2f}s")

sequential_total = time.time() - start_time
print(f"Sequential total: {sequential_total:.2f}s\n")

# Parallel compilation
print("=== Parallel Compilation ===")
parallel_start = time.time()
parallel_results = []

with ThreadPoolExecutor(max_workers=len(confspace_files)) as executor:
    futures = {executor.submit(compile_single, f): f for f in confspace_files}
    
    for future in as_completed(futures):
        confspace_file = futures[future]
        try:
            result = future.result()
            parallel_results.append(result)
            if result['error']:
                print(f"  ERROR ({confspace_file}): {result['error']}")
            else:
                print(f"  Completed {confspace_file}: {result['total_time']:.2f}s")
        except Exception as e:
            print(f"  EXCEPTION ({confspace_file}): {e}")

parallel_total = time.time() - parallel_start
print(f"Parallel total: {parallel_total:.2f}s\n")

profiler.disable()

# Save profile
profile_file = output_dir / f"{test_name}_parallel_profile.prof"
profiler.dump_stats(str(profile_file))

# Generate comparison report
with open(output_dir / f"{test_name}_parallel_comparison.txt", 'w') as f:
    f.write("Parallel vs Sequential Compilation Comparison\n")
    f.write("=" * 60 + "\n\n")
    
    f.write("Sequential Results:\n")
    f.write("-" * 60 + "\n")
    for result in sequential_results:
        f.write(f"  {result['file']}:\n")
        if result['error']:
            f.write(f"    ERROR: {result['error']}\n")
        else:
            f.write(f"    Load: {result['load_time']:.2f}s\n")
            f.write(f"    Compile: {result['compile_time']:.2f}s\n")
            f.write(f"    Save: {result['save_time']:.2f}s\n")
            f.write(f"    Total: {result['total_time']:.2f}s\n")
    f.write(f"\nSequential Total: {sequential_total:.2f}s\n\n")
    
    f.write("Parallel Results:\n")
    f.write("-" * 60 + "\n")
    for result in parallel_results:
        f.write(f"  {result['file']}:\n")
        if result['error']:
            f.write(f"    ERROR: {result['error']}\n")
        else:
            f.write(f"    Load: {result['load_time']:.2f}s\n")
            f.write(f"    Compile: {result['compile_time']:.2f}s\n")
            f.write(f"    Save: {result['save_time']:.2f}s\n")
            f.write(f"    Total: {result['total_time']:.2f}s\n")
    f.write(f"\nParallel Total: {parallel_total:.2f}s\n\n")
    
    if parallel_total > 0:
        speedup = sequential_total / parallel_total
        f.write(f"Speedup: {speedup:.2f}x\n")
        f.write(f"Time saved: {sequential_total - parallel_total:.2f}s\n")
        f.write(f"Efficiency: {speedup/len(confspace_files)*100:.1f}%\n")

print(f"\n=== Comparison Complete ===")
print(f"Sequential: {sequential_total:.2f}s")
print(f"Parallel: {parallel_total:.2f}s")
if parallel_total > 0:
    speedup = sequential_total / parallel_total
    print(f"Speedup: {speedup:.2f}x")
print(f"\nResults: {output_dir}/{test_name}_parallel_comparison.txt")
PYEOF

# Run profiling
echo "Running parallel compilation profiling..."
cd "$REPO_ROOT"
CCKSTAR_DIR="$CCKSTAR_DIR" \
python3 "$OUTPUT_DIR/$TEST_NAME/profile_parallel.py" \
    $CONFSPACE_FILES \
    "$OUTPUT_DIR/$TEST_NAME" \
    "$TEST_NAME" \
    2>&1 | tee "$OUTPUT_DIR/$TEST_NAME/parallel_output.txt"

echo ""
echo "=== Profiling Complete ==="
echo "Results: $OUTPUT_DIR/$TEST_NAME"

