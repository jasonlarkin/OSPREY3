#!/bin/bash
# Profile multiple K* test cases to understand GC behavior variations
# Runs different system sizes and configurations to compare memory patterns

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/pipeline_analysis/memory/multi_case"
ANALYSIS_SCRIPT="$SCRIPT_DIR/analyze_pipeline_memory.py"

# Create output directory
mkdir -p "$OUTPUT_DIR"

echo "=== Profiling Multiple K* Test Cases ==="
echo "Project root: $PROJECT_ROOT"
echo "Output directory: $OUTPUT_DIR"
echo ""

# Test cases to profile
# Format: "test_name:test_method:description"
TEST_CASES=(
    "2RL0:test2RL0:Standard 2RL0 (larger system, multiple mutations)"
    "2RL0_one_mutant:test2RL0OnlyOneMutant:2RL0 with single mutation (reduced sequence space)"
    "1GUA11:test1GUA11:1GUA system (smaller system)"
    "2RL0_no_wt:test2RL0SpaceWithoutWildType:2RL0 without wild type (different sequence space)"
)

# Check if gradlew exists
if [ ! -f "$PROJECT_ROOT/gradlew" ]; then
    echo "ERROR: gradlew not found in $PROJECT_ROOT"
    exit 1
fi

cd "$PROJECT_ROOT" || exit 1
chmod +x gradlew 2>/dev/null || true

# Clear JAVA_TOOL_OPTIONS to avoid conflicts
unset JAVA_TOOL_OPTIONS

# Profile each test case
for test_case in "${TEST_CASES[@]}"; do
    IFS=':' read -r test_name test_method description <<< "$test_case"
    
    echo "=========================================="
    echo "Profiling: $test_name"
    echo "Description: $description"
    echo "Test method: $test_method"
    echo "=========================================="
    
    # Create output directory for this test case
    case_output_dir="$OUTPUT_DIR/$test_name"
    mkdir -p "$case_output_dir"
    
    # Set GC logging for this test
    export GRADLE_OPTS="-Xmx2g -XX:+UseG1GC -Xlog:gc*:file=$case_output_dir/gc.log:time,tags:filecount=5,filesize=10M"
    
    # Run the test with GC logging
    echo "Running test: $test_method"
    if ./gradlew test --tests "edu.duke.cs.osprey.kstar.TestKStar.$test_method" \
        --no-daemon 2>&1 | tee "$case_output_dir/test_output.txt"; then
        echo "✓ Test completed successfully"
    else
        echo "⚠ Test failed or had errors (check output)"
    fi
    
    # Analyze GC log if it exists
    if [ -f "$case_output_dir/gc.log" ]; then
        echo "Analyzing GC log..."
        python3 "$ANALYSIS_SCRIPT" \
            --gc-log "$case_output_dir/gc.log" \
            --stage kstar \
            --output-dir "$case_output_dir" || echo "⚠ GC analysis failed"
    else
        echo "⚠ No GC log generated"
    fi
    
    # Clear GRADLE_OPTS for next test
    unset GRADLE_OPTS
    
    echo ""
done

# Generate comparative analysis
echo "=========================================="
echo "Generating Comparative Analysis"
echo "=========================================="

python3 << PYTHON_EOF
import json
import sys
from pathlib import Path
import matplotlib.pyplot as plt
import numpy as np
from collections import defaultdict

output_dir = Path("$OUTPUT_DIR")
test_cases = []

# Load GC analysis data from each test case
for case_dir in sorted(output_dir.iterdir()):
    if not case_dir.is_dir():
        continue
    
    gc_analysis_file = case_dir / "gc_analysis.md"
    if not gc_analysis_file.exists():
        continue
    
    # Parse summary from markdown
    with open(gc_analysis_file, 'r') as f:
        content = f.read()
        
    case_data = {
        "name": case_dir.name,
        "gc_events": 0,
        "young_gc": 0,
        "full_gc": 0,
        "total_pause_time": 0.0,
        "avg_pause_time": 0.0,
        "max_pause_time": 0.0,
        "duration": 0.0,
        "gc_overhead": 0.0
    }
    
    # Extract values from markdown
    import re
    if match := re.search(r'Total GC Events.*?(\d+)', content):
        case_data["gc_events"] = int(match.group(1))
    if match := re.search(r'Young GC Count.*?(\d+)', content):
        case_data["young_gc"] = int(match.group(1))
    if match := re.search(r'Full GC Count.*?(\d+)', content):
        case_data["full_gc"] = int(match.group(1))
    if match := re.search(r'Total GC Pause Time.*?([\d.]+)', content):
        case_data["total_pause_time"] = float(match.group(1))
    if match := re.search(r'Average GC Pause.*?([\d.]+)', content):
        case_data["avg_pause_time"] = float(match.group(1))
    if match := re.search(r'Maximum GC Pause.*?([\d.]+)', content):
        case_data["max_pause_time"] = float(match.group(1))
    if match := re.search(r'Total Duration.*?([\d.]+)', content):
        case_data["duration"] = float(match.group(1))
    if match := re.search(r'GC Overhead.*?([\d.]+)', content):
        case_data["gc_overhead"] = float(match.group(1))
    
    test_cases.append(case_data)

# Generate comparative plots
if len(test_cases) > 0:
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    names = [c["name"] for c in test_cases]
    
    # 1. GC Event Counts
    ax1 = axes[0, 0]
    x = np.arange(len(names))
    width = 0.35
    young_gc = [c["young_gc"] for c in test_cases]
    full_gc = [c["full_gc"] for c in test_cases]
    ax1.bar(x - width/2, young_gc, width, label='Young GC', alpha=0.7)
    ax1.bar(x + width/2, full_gc, width, label='Full GC', alpha=0.7)
    ax1.set_xlabel("Test Case")
    ax1.set_ylabel("GC Count")
    ax1.set_title("GC Event Counts by Test Case")
    ax1.set_xticks(x)
    ax1.set_xticklabels(names, rotation=45, ha='right')
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # 2. GC Overhead
    ax2 = axes[0, 1]
    gc_overhead = [c["gc_overhead"] for c in test_cases]
    ax2.bar(names, gc_overhead, alpha=0.7, color='orange')
    ax2.set_xlabel("Test Case")
    ax2.set_ylabel("GC Overhead (%)")
    ax2.set_title("GC Overhead by Test Case")
    ax2.set_xticklabels(names, rotation=45, ha='right')
    ax2.grid(True, alpha=0.3)
    
    # 3. Average Pause Time
    ax3 = axes[1, 0]
    avg_pause = [c["avg_pause_time"] for c in test_cases]
    max_pause = [c["max_pause_time"] for c in test_cases]
    x = np.arange(len(names))
    width = 0.35
    ax3.bar(x - width/2, avg_pause, width, label='Average', alpha=0.7)
    ax3.bar(x + width/2, max_pause, width, label='Maximum', alpha=0.7)
    ax3.set_xlabel("Test Case")
    ax3.set_ylabel("Pause Time (ms)")
    ax3.set_title("GC Pause Times by Test Case")
    ax3.set_xticks(x)
    ax3.set_xticklabels(names, rotation=45, ha='right')
    ax3.legend()
    ax3.grid(True, alpha=0.3)
    
    # 4. GC Rate (events per second)
    ax4 = axes[1, 1]
    gc_rates = []
    for c in test_cases:
        if c["duration"] > 0:
            rate = c["gc_events"] / c["duration"]
        else:
            rate = 0
        gc_rates.append(rate)
    ax4.bar(names, gc_rates, alpha=0.7, color='green')
    ax4.set_xlabel("Test Case")
    ax4.set_ylabel("GC Rate (events/sec)")
    ax4.set_title("GC Frequency by Test Case")
    ax4.set_xticklabels(names, rotation=45, ha='right')
    ax4.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plot_file = output_dir / "comparative_gc_analysis.png"
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    print(f"Comparative analysis plot saved to: {plot_file}")
    
    # Generate summary report
    report_file = output_dir / "comparative_summary.md"
    with open(report_file, 'w') as f:
        f.write("# Comparative GC Analysis Across Test Cases\n\n")
        f.write("## Summary\n\n")
        f.write("| Test Case | GC Events | Young GC | Full GC | Avg Pause (ms) | Max Pause (ms) | GC Overhead (%) | GC Rate (events/s) |\n")
        f.write("|-----------|-----------|----------|---------|----------------|----------------|-----------------|-------------------|\n")
        
        for c in test_cases:
            rate = c["gc_events"] / c["duration"] if c["duration"] > 0 else 0
            f.write(f"| {c['name']} | {c['gc_events']} | {c['young_gc']} | {c['full_gc']} | "
                   f"{c['avg_pause_time']:.2f} | {c['max_pause_time']:.2f} | "
                   f"{c['gc_overhead']:.2f} | {rate:.3f} |\n")
        
        f.write("\n## Observations\n\n")
        f.write("Compare GC patterns across different system sizes and configurations to identify:\n")
        f.write("- Which test cases show highest GC pressure\n")
        f.write("- Whether GC behavior scales with system size\n")
        f.write("- Whether allocation-churn reduction would benefit all cases or specific ones (approach TBD)\n")
    
    print(f"Summary report saved to: {report_file}")
else:
    print("No test case data found for comparison")

PYTHON_EOF

echo ""
echo "=== Profiling Complete ==="
echo "Results saved to: $OUTPUT_DIR"
echo ""
echo "Next steps:"
echo "1. Review comparative analysis: $OUTPUT_DIR/comparative_gc_analysis.png"
echo "2. Check summary report: $OUTPUT_DIR/comparative_summary.md"
echo "3. Analyze individual test cases in: $OUTPUT_DIR/*/"

