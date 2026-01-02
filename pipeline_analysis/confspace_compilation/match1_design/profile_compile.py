import sys
import os
import cProfile
import time
import subprocess
import re
from pathlib import Path

confspace_file = sys.argv[1]
output_dir = Path(sys.argv[2])
test_name = sys.argv[3]

# Track LEaP subprocess calls
leap_calls = []
leap_start_times = {}

def track_subprocess():
    """Monkey-patch subprocess to track LEaP calls"""
    original_run = subprocess.run
    
    def tracked_run(*args, **kwargs):
        cmd = args[0] if args else kwargs.get('args', [])
        if isinstance(cmd, (list, tuple)) and len(cmd) > 0:
            cmd_str = ' '.join(str(c) for c in cmd)
            if 'leap' in cmd_str.lower() or 'tleap' in cmd_str.lower():
                start = time.time()
                result = original_run(*args, **kwargs)
                duration = time.time() - start
                leap_calls.append({
                    'command': cmd_str,
                    'duration': duration,
                    'timestamp': start
                })
                return result
        return original_run(*args, **kwargs)
    
    subprocess.run = tracked_run
    return original_run

# Start tracking
original_subprocess = track_subprocess()

# Import OSPREY after patching - must start JVM first
sys.path.insert(0, str(Path(os.environ.get('CCKSTAR_DIR', '.')).resolve()))
import jpype
if not jpype.isJVMStarted():
    import osprey
    osprey.start()
import osprey.prep

# Profile compilation
profiler = cProfile.Profile()
profiler.enable()

start_time = time.time()

try:
    # Load confspace
    print(f"Loading confspace: {confspace_file}")
    load_start = time.time()
    with open(confspace_file, 'r') as f:
        confspace_content = f.read()
    confspace = osprey.prep.loadConfSpace(confspace_content)
    load_time = time.time() - load_start
    print(f"Load time: {load_time:.2f}s")
    
    # Create compiler
    print("Creating compiler...")
    compiler = osprey.prep.ConfSpaceCompiler(confspace)
    compiler.getForcefields().add(osprey.prep.Forcefield.Amber96)
    compiler.getForcefields().add(osprey.prep.Forcefield.EEF1)
    
    # Compile (need LocalService for AmberTools)
    print("Compiling...")
    compile_start = time.time()
    
    with osprey.prep.LocalService():
        progress = compiler.compile()
        progress.printUntilFinish(10000)
        report = progress.getReport()
    compile_time = time.time() - compile_start
    
    if report.getError() is not None:
        raise Exception('Compilation failed', report.getError())
    
    # Save compiled
    print("Saving compiled confspace...")
    save_start = time.time()
    save_path = confspace_file.replace('.confspace', '.ccsx')
    compiled_bytes = osprey.prep.saveCompiledConfSpace(report.getCompiled())
    with open(save_path, 'wb') as f:
        f.write(compiled_bytes)
    save_time = time.time() - save_start
    
    total_time = time.time() - start_time
    
    print(f"\n=== Compilation Complete ===")
    print(f"Load time: {load_time:.2f}s")
    print(f"Compile time: {compile_time:.2f}s")
    print(f"Save time: {save_time:.2f}s")
    print(f"Total time: {total_time:.2f}s")
    print(f"LEaP calls: {len(leap_calls)}")
    
    # Write LEaP call analysis
    with open(output_dir / f"{test_name}_leap_calls.txt", 'w') as f:
        f.write("LEaP Subprocess Calls\n")
        f.write("=" * 60 + "\n\n")
        total_leap_time = 0
        for i, call in enumerate(leap_calls, 1):
            f.write(f"Call {i}:\n")
            f.write(f"  Command: {call['command']}\n")
            f.write(f"  Duration: {call['duration']:.3f}s\n")
            f.write(f"  Timestamp: {call['timestamp']:.3f}s\n\n")
            total_leap_time += call['duration']
        f.write(f"\nTotal LEaP time: {total_leap_time:.2f}s\n")
        f.write(f"Average LEaP call: {total_leap_time/len(leap_calls):.3f}s\n" if leap_calls else "No LEaP calls\n")
    
    # Write timing summary
    with open(output_dir / f"{test_name}_timing.txt", 'w') as f:
        f.write("ConfSpace Compilation Timing\n")
        f.write("=" * 60 + "\n\n")
        f.write(f"ConfSpace file: {confspace_file}\n")
        f.write(f"Load time: {load_time:.2f}s\n")
        f.write(f"Compile time: {compile_time:.2f}s\n")
        f.write(f"Save time: {save_time:.2f}s\n")
        f.write(f"Total time: {total_time:.2f}s\n")
        f.write(f"LEaP calls: {len(leap_calls)}\n")
        if leap_calls:
            total_leap = sum(c['duration'] for c in leap_calls)
            f.write(f"Total LEaP time: {total_leap:.2f}s\n")
            f.write(f"LEaP overhead: {total_leap/compile_time*100:.1f}% of compile time\n")
    
except Exception as e:
    print(f"ERROR: {e}")
    import traceback
    traceback.print_exc()
    sys.exit(1)

profiler.disable()

# Save profile
profile_file = output_dir / f"{test_name}_profile.prof"
profiler.dump_stats(str(profile_file))
print(f"\nProfile saved: {profile_file}")

# Generate reports
import pstats
stats = pstats.Stats(str(profile_file))

# Top 50 cumulative
with open(output_dir / f"{test_name}_top50_cumulative.txt", 'w') as f:
    old_stdout = sys.stdout
    sys.stdout = f
    stats.sort_stats('cumulative')
    stats.print_stats(50)
    sys.stdout = old_stdout

# Top 50 time
with open(output_dir / f"{test_name}_top50_time.txt", 'w') as f:
    old_stdout = sys.stdout
    sys.stdout = f
    stats.sort_stats('time')
    stats.print_stats(50)
    sys.stdout = old_stdout

print(f"Reports generated in: {output_dir}")
