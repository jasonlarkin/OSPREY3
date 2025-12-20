#!/usr/bin/env python3
"""
Extract performance metrics from TestNativeConfEnergyCalculator test output and compare between branches.
Integrates with existing performance benchmarking infrastructure in scripts/tools/.

Outputs:
- JSON comparison report
- CSV format compatible with scripts/tools/summarize_benchmark_results.py
- Human-readable summary

Usage:
    python3 scripts/extract_performance_metrics.py <baseline_file> <test_file> [output_json] [output_csv]
"""

import re
import sys
import json
import csv
from pathlib import Path
from typing import Dict, List, Optional
from collections import defaultdict

def parse_timing_line(line: str) -> Optional[Dict[str, any]]:
    """
    Parse a timing line from test output.
    Format: "operation: N confs in X.XXs (Y.YY confs/s)" or "operation: N confs in X.XX ms (Y.YY confs/s)"
    """
    # Pattern: "assign: 15 confs in 5.65 s (2.65 confs/s)" or "calcEnergy_all: 7 confs in 335.65 ms (20.85 confs/s)"
    # Handles optional space before unit, and both "s" and "ms" units
    pattern = r'(\w+):\s+(\d+)\s+confs\s+in\s+([\d.]+)\s*(ms|s)\s+\(([\d.]+)\s+confs/s\)'
    match = re.search(pattern, line)
    
    if match:
        time_value = float(match.group(3))
        unit = match.group(4)
        # Convert ms to seconds
        if unit == 'ms':
            time_seconds = time_value / 1000.0
        else:
            time_seconds = time_value
        
        return {
            'operation': match.group(1),
            'confs': int(match.group(2)),
            'time_seconds': time_seconds,
            'throughput': float(match.group(5))
        }
    return None

def extract_total_test_time(file_path: Path) -> Optional[Dict[str, any]]:
    """
    Extract total test execution time from Gradle output as fallback.
    Looks for "BUILD SUCCESSFUL in Xm Ys" pattern.
    """
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
            
        # Pattern: "BUILD SUCCESSFUL in 2m 27s" or "BUILD SUCCESSFUL in 42s"
        pattern = r'BUILD SUCCESSFUL in (?:(\d+)m\s+)?(\d+)s'
        match = re.search(pattern, content)
        
        if match:
            minutes = int(match.group(1)) if match.group(1) else 0
            seconds = int(match.group(2))
            total_seconds = minutes * 60 + seconds
            
            return {
                'operation': 'total_test_execution',
                'confs': 4,  # We run 4 specific tests
                'time_seconds': total_seconds,
                'throughput': 4.0 / total_seconds if total_seconds > 0 else 0
            }
    except Exception as e:
        print(f"Error extracting total test time from {file_path}: {e}", file=sys.stderr)
    
    return None

def extract_metrics_from_file(file_path: Path) -> List[Dict[str, any]]:
    """Extract all timing metrics from a test output file."""
    metrics = []
    
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            for line in f:
                metric = parse_timing_line(line)
                if metric:
                    metrics.append(metric)
        
        # If no detailed timing found, use total test execution time as fallback
        if not metrics:
            total_time = extract_total_test_time(file_path)
            if total_time:
                metrics.append(total_time)
                print(f"Note: Using total test execution time as fallback (detailed timing not found)")
    except FileNotFoundError:
        print(f"Warning: File not found: {file_path}", file=sys.stderr)
    except Exception as e:
        print(f"Error reading {file_path}: {e}", file=sys.stderr)
    
    return metrics

def compare_metrics(baseline_metrics: List[Dict], test_metrics: List[Dict]) -> Dict[str, any]:
    """Compare metrics between baseline and test runs."""
    comparison = {
        'baseline_count': len(baseline_metrics),
        'test_count': len(test_metrics),
        'comparisons': []
    }
    
    # Group by operation
    baseline_by_op = {}
    for m in baseline_metrics:
        op = m['operation']
        if op not in baseline_by_op:
            baseline_by_op[op] = []
        baseline_by_op[op].append(m)
    
    test_by_op = {}
    for m in test_metrics:
        op = m['operation']
        if op not in test_by_op:
            test_by_op[op] = []
        test_by_op[op].append(m)
    
    # Compare each operation
    all_ops = set(baseline_by_op.keys()) | set(test_by_op.keys())
    
    for op in sorted(all_ops):
        baseline_ops = baseline_by_op.get(op, [])
        test_ops = test_by_op.get(op, [])
        
        if not baseline_ops:
            comparison['comparisons'].append({
                'operation': op,
                'status': 'new',
                'message': f'Operation {op} only in test branch'
            })
            continue
        
        if not test_ops:
            comparison['comparisons'].append({
                'operation': op,
                'status': 'missing',
                'message': f'Operation {op} missing in test branch'
            })
            continue
        
        # Compare average throughput
        baseline_avg = sum(m['throughput'] for m in baseline_ops) / len(baseline_ops)
        test_avg = sum(m['throughput'] for m in test_ops) / len(test_ops)
        
        speedup = test_avg / baseline_avg if baseline_avg > 0 else 0
        percent_change = (speedup - 1.0) * 100
        
        comparison['comparisons'].append({
            'operation': op,
            'status': 'compared',
            'baseline_throughput': round(baseline_avg, 2),
            'test_throughput': round(test_avg, 2),
            'speedup': round(speedup, 3),
            'percent_change': round(percent_change, 2),
            'baseline_count': len(baseline_ops),
            'test_count': len(test_ops)
        })
    
    return comparison

def write_csv_output(baseline_metrics: List[Dict], test_metrics: List[Dict], output_csv: Path):
    """Write metrics in CSV format compatible with scripts/tools/summarize_benchmark_results.py"""
    with open(output_csv, 'w', newline='') as f:
        writer = csv.writer(f)
        # Header compatible with existing tools
        writer.writerow(['version', 'system_size', 'operation', 'time_us_per_iter', 'time_total_ms', 'run_id'])
        
        # Write baseline metrics as "main" version
        for i, m in enumerate(baseline_metrics):
            # Convert throughput (confs/s) to time per conf (us)
            time_us_per_conf = (1.0 / m['throughput']) * 1_000_000 if m['throughput'] > 0 else 0
            time_total_ms = m['time_seconds'] * 1000
            writer.writerow(['main', 'test', m['operation'], f'{time_us_per_conf:.3f}', f'{time_total_ms:.3f}', i])
        
        # Write test metrics as "develop" version
        for i, m in enumerate(test_metrics):
            time_us_per_conf = (1.0 / m['throughput']) * 1_000_000 if m['throughput'] > 0 else 0
            time_total_ms = m['time_seconds'] * 1000
            writer.writerow(['develop', 'test', m['operation'], f'{time_us_per_conf:.3f}', f'{time_total_ms:.3f}', i])

def main():
    if len(sys.argv) < 3:
        print("Usage: extract_performance_metrics.py <baseline_file> <test_file> [output_json] [output_csv]")
        print("  baseline_file: Test output from main branch")
        print("  test_file: Test output from develop branch")
        print("  output_json: Optional JSON comparison report")
        print("  output_csv: Optional CSV output (compatible with scripts/tools/summarize_benchmark_results.py)")
        sys.exit(1)
    
    baseline_file = Path(sys.argv[1])
    test_file = Path(sys.argv[2])
    output_json = Path(sys.argv[3]) if len(sys.argv) > 3 else None
    output_csv = Path(sys.argv[4]) if len(sys.argv) > 4 else None
    
    print(f"Extracting metrics from baseline: {baseline_file}")
    baseline_metrics = extract_metrics_from_file(baseline_file)
    print(f"Found {len(baseline_metrics)} timing metrics in baseline")
    
    print(f"Extracting metrics from test: {test_file}")
    test_metrics = extract_metrics_from_file(test_file)
    print(f"Found {len(test_metrics)} timing metrics in test")
    
    comparison = compare_metrics(baseline_metrics, test_metrics)
    
    # Print comparison
    print("\n=== Performance Comparison ===")
    for comp in comparison['comparisons']:
        if comp['status'] == 'compared':
            change = comp['percent_change']
            symbol = '+' if change >= 0 else ''
            print(f"{comp['operation']:20s}: {comp['baseline_throughput']:8.2f} -> {comp['test_throughput']:8.2f} confs/s "
                  f"({symbol}{change:+.2f}%, {comp['speedup']:.3f}x)")
        else:
            print(f"{comp['operation']:20s}: {comp['message']}")
    
    # Save JSON if requested
    if output_json:
        output_data = {
            'baseline_metrics': baseline_metrics,
            'test_metrics': test_metrics,
            'comparison': comparison
        }
        with open(output_json, 'w') as f:
            json.dump(output_data, f, indent=2)
        print(f"\nDetailed comparison saved to: {output_json}")
    
    # Save CSV if requested (compatible with existing tools)
    if output_csv:
        write_csv_output(baseline_metrics, test_metrics, output_csv)
        print(f"CSV output saved to: {output_csv}")
        print("  (Compatible with scripts/tools/summarize_benchmark_results.py)")
    
    # Check for regressions (>5% slower)
    regressions = [c for c in comparison['comparisons'] 
                   if c['status'] == 'compared' and c['percent_change'] < -5.0]
    
    if regressions:
        print("\n⚠️  PERFORMANCE REGRESSIONS DETECTED (>5% slower):")
        for r in regressions:
            print(f"  {r['operation']}: {r['percent_change']:.2f}% slower")
        sys.exit(1)
    else:
        print("\n✓ No significant performance regressions detected")

if __name__ == '__main__':
    main()

