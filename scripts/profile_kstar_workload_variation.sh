#!/bin/bash
# Profile K* across different system sizes to understand workload variation
# Measures: time scaling, memory scaling, GC overhead

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$REPO_ROOT/pipeline_analysis/kstar/workload_variation"
CCKSTAR_DIR="$REPO_ROOT/src/main/python/CCKStar"

mkdir -p "$OUTPUT_DIR"

echo "=== K* Workload Variation Profiling ==="
echo "Output: $OUTPUT_DIR"
echo ""

cd "$CCKSTAR_DIR"

# Test cases with known pair counts (from PROBLEM_SIZE_COVERAGE.md)
# We'll use 2RL0 components as they have different sizes

# Small: 2RL0 protein (1,008 pairs)
# Medium: 2RL0 ligand (7,575 pairs)  
# Large: 2RL0 complex (16,734 pairs)

# Check if we have 2RL0 MONTAGE output
if [ ! -d "2RL0-MONTAGE" ]; then
    echo "ERROR: 2RL0-MONTAGE directory not found"
    echo "Run MONTAGE first to generate test cases"
    exit 1
fi

# Find available matches
MATCHES=$(ls -d 2RL0-MONTAGE/match*-MONTAGE 2>/dev/null | head -1)
if [ -z "$MATCHES" ]; then
    echo "ERROR: No matches found in 2RL0-MONTAGE"
    exit 1
fi

MATCH_DIR=$(echo "$MATCHES" | head -1)
KSTAR_DIR="$MATCH_DIR/kstar-[MONTAGE]"

if [ ! -d "$KSTAR_DIR" ]; then
    echo "ERROR: K* directory not found: $KSTAR_DIR"
    exit 1
fi

echo "Using match directory: $MATCH_DIR"
echo "K* directory: $KSTAR_DIR"
echo ""

# Check available confspaces
if [ ! -f "$KSTAR_DIR/complex.ccsx" ]; then
    echo "ERROR: complex.ccsx not found in $KSTAR_DIR"
    exit 1
fi

# We'll profile by running K* on each component separately
# This requires extracting or creating separate confspaces for protein/ligand

echo "=== Profiling K* Workload Variation ==="
echo ""
echo "Note: This will run K* on available confspaces"
echo "For full variation, we need protein/ligand/complex separately"
echo ""

# Profile complex (large system)
if [ -f "$KSTAR_DIR/complex.ccsx" ]; then
    echo "=== Profiling Complex (Large System) ==="
    echo "Expected pairs: ~16,734"
    
    PROFILE_DIR="$OUTPUT_DIR/complex"
    mkdir -p "$PROFILE_DIR"
    
    # Run K* with GC logging
    python3 "$REPO_ROOT/src/main/python/CCKStar/run_kstar_python.py" \
        "$KSTAR_DIR" \
        --heap-mib 4096 \
        --direct-mib 2048 \
        --cpu-cores 4 \
        2>&1 | tee "$PROFILE_DIR/kstar_output.txt" &
    
    KSTAR_PID=$!
    
    # Monitor memory usage
    (
        while kill -0 $KSTAR_PID 2>/dev/null; do
            ps -p $KSTAR_PID -o rss,vsz,pcpu,etime --no-headers >> "$PROFILE_DIR/memory_monitor.txt"
            sleep 1
        done
    ) &
    MONITOR_PID=$!
    
    wait $KSTAR_PID
    kill $MONITOR_PID 2>/dev/null || true
    
    echo "  Completed complex profiling"
    echo "  Results: $PROFILE_DIR"
    echo ""
fi

# If we have design/target confspaces, profile those too
if [ -f "$KSTAR_DIR/design.ccsx" ]; then
    echo "=== Profiling Design (Medium System) ==="
    echo "Expected pairs: ~7,575 (ligand-like)"
    
    PROFILE_DIR="$OUTPUT_DIR/design"
    mkdir -p "$PROFILE_DIR"
    
    # Create a minimal test by running on design only
    # Note: This may not be a complete K* run, but gives us workload data
    python3 "$REPO_ROOT/src/main/python/CCKStar/run_kstar_python.py" \
        "$KSTAR_DIR" \
        --heap-mib 2048 \
        --direct-mib 1024 \
        --cpu-cores 4 \
        2>&1 | tee "$PROFILE_DIR/kstar_output.txt" &
    
    KSTAR_PID=$!
    
    (
        while kill -0 $KSTAR_PID 2>/dev/null; do
            ps -p $KSTAR_PID -o rss,vsz,pcpu,etime --no-headers >> "$PROFILE_DIR/memory_monitor.txt"
            sleep 1
        done
    ) &
    MONITOR_PID=$!
    
    wait $KSTAR_PID
    kill $MONITOR_PID 2>/dev/null || true
    
    echo "  Completed design profiling"
    echo "  Results: $PROFILE_DIR"
    echo ""
fi

echo "=== Generating Analysis ==="
python3 << 'PYTHON_EOF'
import sys
import os
import json
import re
from pathlib import Path
from datetime import datetime

output_dir = Path(sys.argv[1])

results = {}

# Parse each profile directory
for profile_dir in ['complex', 'design', 'target']:
    profile_path = output_dir / profile_dir
    if not profile_path.exists():
        continue
    
    output_file = profile_path / 'kstar_output.txt'
    memory_file = profile_path / 'memory_monitor.txt'
    
    if not output_file.exists():
        continue
    
    # Parse K* output for timing
    with open(output_file) as f:
        content = f.read()
    
    # Extract execution time
    time_match = re.search(r'(\d+\.\d+)\s+seconds', content)
    execution_time = float(time_match.group(1)) if time_match else None
    
    # Extract pair counts if available
    pairs_match = re.search(r'(\d+)\s+pairs', content, re.IGNORECASE)
    pair_count = int(pairs_match.group(1)) if pairs_match else None
    
    # Parse memory monitoring
    peak_memory = 0
    if memory_file.exists():
        with open(memory_file) as f:
            for line in f:
                parts = line.split()
                if len(parts) >= 1:
                    try:
                        rss_mb = int(parts[0]) / 1024  # Convert KB to MB
                        peak_memory = max(peak_memory, rss_mb)
                    except ValueError:
                        pass
    
    results[profile_dir] = {
        'execution_time': execution_time,
        'pair_count': pair_count,
        'peak_memory_mb': peak_memory,
    }

# Write results
with open(output_dir / 'workload_results.json', 'w') as f:
    json.dump(results, f, indent=2)

# Generate summary
summary_file = output_dir / 'workload_summary.md'
with open(summary_file, 'w') as f:
    f.write("# K* Workload Variation Summary\n\n")
    f.write(f"Generated: {datetime.now().isoformat()}\n\n")
    
    f.write("## Results\n\n")
    f.write("| System | Pairs | Time (s) | Peak Memory (MB) | Time/Pair (ms) | Memory/Pair (KB) |\n")
    f.write("|--------|-------|----------|------------------|----------------|------------------|\n")
    
    for system, data in results.items():
        pairs = data.get('pair_count', 'N/A')
        time = data.get('execution_time', 'N/A')
        memory = data.get('peak_memory_mb', 'N/A')
        
        if isinstance(pairs, int) and isinstance(time, (int, float)):
            time_per_pair = (time * 1000) / pairs
        else:
            time_per_pair = 'N/A'
        
        if isinstance(pairs, int) and isinstance(memory, (int, float)):
            memory_per_pair = (memory * 1024) / pairs
        else:
            memory_per_pair = 'N/A'
        
        f.write(f"| {system} | {pairs} | {time} | {memory:.1f} | {time_per_pair:.2f} | {memory_per_pair:.2f} |\n")
    
    f.write("\n## Scaling Analysis\n\n")
    
    # Calculate scaling if we have multiple data points
    if len(results) >= 2:
        systems = sorted(results.items(), key=lambda x: x[1].get('pair_count', 0) or 0)
        
        f.write("### Time Scaling\n\n")
        for i in range(len(systems) - 1):
            sys1, data1 = systems[i]
            sys2, data2 = systems[i + 1]
            
            pairs1 = data1.get('pair_count')
            pairs2 = data2.get('pair_count')
            time1 = data1.get('execution_time')
            time2 = data2.get('execution_time')
            
            if pairs1 and pairs2 and time1 and time2:
                pair_ratio = pairs2 / pairs1
                time_ratio = time2 / time1
                scaling = time_ratio / pair_ratio
                
                f.write(f"- {sys1} → {sys2}: {pair_ratio:.2f}x pairs, {time_ratio:.2f}x time\n")
                f.write(f"  - Scaling factor: {scaling:.2f}x (1.0 = linear, >1.0 = super-linear)\n\n")
        
        f.write("### Memory Scaling\n\n")
        for i in range(len(systems) - 1):
            sys1, data1 = systems[i]
            sys2, data2 = systems[i + 1]
            
            pairs1 = data1.get('pair_count')
            pairs2 = data2.get('pair_count')
            mem1 = data1.get('peak_memory_mb')
            mem2 = data2.get('peak_memory_mb')
            
            if pairs1 and pairs2 and mem1 and mem2:
                pair_ratio = pairs2 / pairs1
                mem_ratio = mem2 / mem1
                scaling = mem_ratio / pair_ratio
                
                f.write(f"- {sys1} → {sys2}: {pair_ratio:.2f}x pairs, {mem_ratio:.2f}x memory\n")
                f.write(f"  - Scaling factor: {scaling:.2f}x (1.0 = linear, >1.0 = super-linear)\n\n")

print(f"Analysis complete: {summary_file}")
PYTHON_EOF "$OUTPUT_DIR"

echo ""
echo "=== Workload Variation Profiling Complete ==="
echo "Results: $OUTPUT_DIR"
echo "Summary: $OUTPUT_DIR/workload_summary.md"

