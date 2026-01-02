#!/usr/bin/env python3
"""
Analyze CPU profiling data from perf to identify bottlenecks.
Parses perf stat and perf report output to find:
- CPU hotspots
- Cache miss rates
- Branch prediction issues
- Memory access patterns
- Function call frequencies
"""

import argparse
import re
import sys
from pathlib import Path
from typing import Dict, List, Tuple
from collections import defaultdict
import matplotlib.pyplot as plt
import numpy as np

def parse_perf_stat(stat_file: Path) -> Dict:
    """Parse perf stat output."""
    stats = {}
    
    try:
        with open(stat_file, 'r') as f:
            content = f.read()
            
        # Parse various metrics
        patterns = {
            'cycles': r'(\d+(?:,\d+)*)\s+cycles',
            'instructions': r'(\d+(?:,\d+)*)\s+instructions',
            'cache_references': r'(\d+(?:,\d+)*)\s+cache-references',
            'cache_misses': r'(\d+(?:,\d+)*)\s+cache-misses',
            'branch_instructions': r'(\d+(?:,\d+)*)\s+branch-instructions',
            'branch_misses': r'(\d+(?:,\d+)*)\s+branch-misses',
            'page_faults': r'(\d+(?:,\d+)*)\s+page-faults',
            'task_clock': r'(\d+\.\d+)\s+msec\s+task-clock',
            'cpu_utilization': r'(\d+\.\d+)%\s+CPU',
        }
        
        for key, pattern in patterns.items():
            match = re.search(pattern, content)
            if match:
                value_str = match.group(1).replace(',', '')
                try:
                    if '.' in value_str:
                        stats[key] = float(value_str)
                    else:
                        stats[key] = int(value_str)
                except ValueError:
                    pass
        
        # Calculate derived metrics
        if 'instructions' in stats and 'cycles' in stats:
            stats['ipc'] = stats['instructions'] / stats['cycles'] if stats['cycles'] > 0 else 0
        
        if 'cache_references' in stats and 'cache_misses' in stats:
            if stats['cache_references'] > 0:
                stats['cache_miss_rate'] = (stats['cache_misses'] / stats['cache_references']) * 100
        
        if 'branch_instructions' in stats and 'branch_misses' in stats:
            if stats['branch_instructions'] > 0:
                stats['branch_miss_rate'] = (stats['branch_misses'] / stats['branch_instructions']) * 100
    
    except Exception as e:
        print(f"Error parsing perf stat: {e}")
    
    return stats

def parse_perf_report(report_file: Path) -> List[Tuple[str, float, str]]:
    """Parse perf report to extract top functions."""
    functions = []
    
    try:
        with open(report_file, 'r') as f:
            lines = f.readlines()
        
        # Look for function entries (format varies)
        # Example: "  12.34%  libjvm.so  [.] some_function"
        for line in lines:
            # Match percentage and function name
            match = re.match(r'\s*(\d+\.\d+)%\s+.*?\s+\[\.\]\s+(.+)', line)
            if match:
                percentage = float(match.group(1))
                function = match.group(2).strip()
                functions.append((function, percentage, line.strip()))
            
            # Alternative format: "  12.34%  some_function"
            match = re.match(r'\s*(\d+\.\d+)%\s+(.+)', line)
            if match and '[.]' not in line and len(functions) < 50:  # Avoid duplicates
                percentage = float(match.group(1))
                function = match.group(2).strip()
                if function and not function.startswith('['):
                    functions.append((function, percentage, line.strip()))
    
    except Exception as e:
        print(f"Error parsing perf report: {e}")
    
    # Sort by percentage
    functions.sort(key=lambda x: x[1], reverse=True)
    return functions[:20]  # Top 20

def analyze_cpu_profiles(output_dir: Path):
    """Analyze all CPU profiles in the output directory."""
    test_cases = []
    
    # Find all test case directories
    for case_dir in sorted(output_dir.iterdir()):
        if not case_dir.is_dir():
            continue
        
        case_data = {
            'name': case_dir.name,
            'stats': {},
            'top_functions': []
        }
        
        # Parse perf stat
        stat_file = case_dir / 'perf_stat.txt'
        if stat_file.exists():
            case_data['stats'] = parse_perf_stat(stat_file)
        
        # Parse perf report
        report_file = case_dir / 'perf_report.txt'
        if report_file.exists():
            case_data['top_functions'] = parse_perf_report(report_file)
        
        if case_data['stats'] or case_data['top_functions']:
            test_cases.append(case_data)
    
    return test_cases

def generate_analysis_report(test_cases: List[Dict], output_dir: Path):
    """Generate markdown report from CPU analysis."""
    report_file = output_dir / 'cpu_analysis_report.md'
    
    with open(report_file, 'w') as f:
        f.write("# CPU Bottleneck Analysis Report\n\n")
        
        if not test_cases:
            f.write("No CPU profiling data found.\n")
            return
        
        f.write("## Summary Statistics\n\n")
        f.write("| Test Case | IPC | Cache Miss Rate (%) | Branch Miss Rate (%) | Task Clock (ms) |\n")
        f.write("|-----------|-----|---------------------|---------------------|-----------------|\n")
        
        for case in test_cases:
            stats = case['stats']
            ipc = stats.get('ipc', 0)
            cache_miss = stats.get('cache_miss_rate', 0)
            branch_miss = stats.get('branch_miss_rate', 0)
            task_clock = stats.get('task_clock', 0)
            
            f.write(f"| {case['name']} | {ipc:.2f} | {cache_miss:.2f} | {branch_miss:.2f} | {task_clock:.1f} |\n")
        
        f.write("\n## Top Functions by CPU Time\n\n")
        
        for case in test_cases:
            f.write(f"### {case['name']}\n\n")
            
            if case['top_functions']:
                f.write("| Function | CPU % |\n")
                f.write("|----------|-------|\n")
                for func, pct, _ in case['top_functions'][:10]:
                    # Truncate long function names
                    func_display = func[:60] + '...' if len(func) > 60 else func
                    f.write(f"| `{func_display}` | {pct:.2f}% |\n")
            else:
                f.write("No function data available.\n")
            
            f.write("\n")
        
        f.write("## Key Metrics Explained\n\n")
        f.write("- **IPC (Instructions Per Cycle)**: Higher is better. < 1.0 indicates CPU stalls.\n")
        f.write("- **Cache Miss Rate**: Lower is better. > 5% may indicate memory access issues.\n")
        f.write("- **Branch Miss Rate**: Lower is better. > 2% may indicate branch prediction issues.\n")
        f.write("\n")
        f.write("## Bottleneck Identification\n\n")
        f.write("Look for:\n")
        f.write("1. **Low IPC** (< 1.0): CPU waiting on memory or pipeline stalls\n")
        f.write("2. **High cache miss rate** (> 5%): Poor memory locality\n")
        f.write("3. **Functions with high CPU%**: These are the hotspots to optimize\n")
        f.write("4. **Memory allocation functions** (malloc, new): Consider reducing allocation churn (approach TBD)\n")
        f.write("5. **Energy calculation functions**: May benefit from SIMD/vectorization\n")
    
    print(f"CPU analysis report saved to: {report_file}")

def generate_visualizations(test_cases: List[Dict], output_dir: Path):
    """Generate visualization plots."""
    if not test_cases:
        return
    
    try:
        fig, axes = plt.subplots(2, 2, figsize=(14, 10))
        
        names = [c['name'] for c in test_cases]
        
        # 1. IPC comparison
        ax1 = axes[0, 0]
        ipc_values = [c['stats'].get('ipc', 0) for c in test_cases]
        ax1.bar(names, ipc_values, alpha=0.7, color='steelblue')
        ax1.axhline(y=1.0, color='red', linestyle='--', label='IPC = 1.0 (baseline)')
        ax1.set_ylabel('IPC (Instructions Per Cycle)')
        ax1.set_title('CPU Efficiency (IPC)')
        ax1.set_xticklabels(names, rotation=45, ha='right')
        ax1.legend()
        ax1.grid(True, alpha=0.3)
        
        # 2. Cache miss rate
        ax2 = axes[0, 1]
        cache_miss_rates = [c['stats'].get('cache_miss_rate', 0) for c in test_cases]
        ax2.bar(names, cache_miss_rates, alpha=0.7, color='orange')
        ax2.axhline(y=5.0, color='red', linestyle='--', label='5% threshold')
        ax2.set_ylabel('Cache Miss Rate (%)')
        ax2.set_title('Memory Cache Performance')
        ax2.set_xticklabels(names, rotation=45, ha='right')
        ax2.legend()
        ax2.grid(True, alpha=0.3)
        
        # 3. Branch miss rate
        ax3 = axes[1, 0]
        branch_miss_rates = [c['stats'].get('branch_miss_rate', 0) for c in test_cases]
        ax3.bar(names, branch_miss_rates, alpha=0.7, color='green')
        ax3.axhline(y=2.0, color='red', linestyle='--', label='2% threshold')
        ax3.set_ylabel('Branch Miss Rate (%)')
        ax3.set_title('Branch Prediction Performance')
        ax3.set_xticklabels(names, rotation=45, ha='right')
        ax3.legend()
        ax3.grid(True, alpha=0.3)
        
        # 4. Top functions (stacked bar or pie for one case)
        ax4 = axes[1, 1]
        if test_cases and test_cases[0]['top_functions']:
            top_funcs = test_cases[0]['top_functions'][:5]
            func_names = [f[0][:30] + '...' if len(f[0]) > 30 else f[0] for f in top_funcs]
            percentages = [f[1] for f in top_funcs]
            ax4.barh(func_names, percentages, alpha=0.7, color='purple')
            ax4.set_xlabel('CPU Time (%)')
            ax4.set_title(f"Top Functions: {test_cases[0]['name']}")
            ax4.grid(True, alpha=0.3, axis='x')
        else:
            ax4.text(0.5, 0.5, 'No function data', ha='center', va='center', transform=ax4.transAxes)
            ax4.set_title('Top Functions')
        
        plt.tight_layout()
        plot_file = output_dir / 'cpu_analysis.png'
        plt.savefig(plot_file, dpi=300, bbox_inches='tight')
        print(f"CPU analysis plot saved to: {plot_file}")
        
    except Exception as e:
        print(f"Warning: Could not generate visualizations: {e}")

def main():
    parser = argparse.ArgumentParser(description="Analyze CPU profiling data from perf")
    parser.add_argument("--output-dir", type=Path, required=True, help="Directory containing perf data")
    
    args = parser.parse_args()
    
    if not args.output_dir.exists():
        print(f"Error: Output directory not found: {args.output_dir}")
        sys.exit(1)
    
    print(f"Analyzing CPU profiles in: {args.output_dir}")
    
    # Check for async-profiler HTML files (interactive flame graphs)
    async_profiler_files = list(args.output_dir.rglob("*.html"))
    if async_profiler_files:
        print(f"\nFound {len(async_profiler_files)} async-profiler HTML profile(s):")
        for html_file in async_profiler_files:
            rel_path = html_file.relative_to(args.output_dir)
            print(f"  - {rel_path}")
        print("\nThese are interactive flame graphs - open in a web browser to view.")
        print("Note: This script analyzes perf data. For async-profiler analysis,")
        print("      view the HTML files directly in your browser.\n")
    
    test_cases = analyze_cpu_profiles(args.output_dir)
    
    if not test_cases:
        if async_profiler_files:
            print("No perf data found, but async-profiler HTML files are available above.")
        else:
            print("No CPU profiling data found.")
        return
    
    generate_analysis_report(test_cases, args.output_dir)
    generate_visualizations(test_cases, args.output_dir)
    
    print(f"\nAnalysis complete! Results in: {args.output_dir}")

if __name__ == '__main__':
    main()

