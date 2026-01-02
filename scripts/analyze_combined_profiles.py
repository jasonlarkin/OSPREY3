#!/usr/bin/env python3
"""
Combined analysis of GC logs, CPU profiles, and allocation profiles.
Correlates findings across all profiling data sources.
"""

import argparse
import json
import re
from pathlib import Path
from typing import Dict, List, Optional
from collections import defaultdict
from datetime import datetime

def parse_gc_analysis(gc_analysis_dir: Path) -> Optional[Dict]:
    """Parse GC analysis markdown report."""
    gc_report = gc_analysis_dir / "gc_analysis.md"
    if not gc_report.exists():
        return None
    
    data = {
        'total_events': 0,
        'young_gc_count': 0,
        'full_gc_count': 0,
        'total_pause_time': 0.0,
        'average_pause': 0.0,
        'max_pause': 0.0,
        'duration': 0.0,
        'gc_overhead': 0.0,
        'peak_heap': 0
    }
    
    with open(gc_report, 'r') as f:
        content = f.read()
    
    # Extract summary metrics
    patterns = {
        'total_events': r'Total GC Events[:\*\s]+(\d+)',
        'young_gc_count': r'Young GC Count[:\*\s]+(\d+)',
        'full_gc_count': r'Full GC Count[:\*\s]+(\d+)',
        'total_pause_time': r'Total GC Pause Time[:\*\s]+([\d.]+)\s*seconds?',
        'average_pause': r'Average GC Pause[:\*\s]+([\d.]+)\s*ms',
        'max_pause': r'Maximum GC Pause[:\*\s]+([\d.]+)\s*ms',
        'duration': r'Total Duration[:\*\s]+([\d.]+)\s*seconds?',
        'gc_overhead': r'GC Overhead[:\*\s]+([\d.]+)%',
    }
    
    for key, pattern in patterns.items():
        match = re.search(pattern, content, re.IGNORECASE)
        if match:
            value = match.group(1)
            if key in ['total_events', 'young_gc_count', 'full_gc_count']:
                data[key] = int(value)
            else:
                data[key] = float(value)
    
    # Extract peak heap
    heap_match = re.search(r'Peak Heap[:\*\s]+(\d+)M', content, re.IGNORECASE)
    if heap_match:
        data['peak_heap'] = int(heap_match.group(1))
    
    return data

def parse_async_profiler_analysis(profile_dir: Path) -> Dict:
    """Parse async-profiler analysis reports."""
    cpu_report = None
    alloc_report = None
    
    # Find CPU and allocation analysis reports
    for report_file in profile_dir.glob("*_cpu_analysis.md"):
        cpu_report = report_file
        break
    
    for report_file in profile_dir.glob("*_allocation_analysis.md"):
        alloc_report = report_file
        break
    
    data = {
        'cpu': None,
        'allocation': None
    }
    
    def parse_report(report_file: Path) -> Optional[Dict]:
        if not report_file or not report_file.exists():
            return None
        
        report_data = {
            'total_samples': 0,
            'app_pct': 0.0,
            'infra_pct': 0.0,
            'hotspots': {}
        }
        
        with open(report_file, 'r') as f:
            content = f.read()
        
        # Extract total samples
        samples_match = re.search(r'Total samples[:\*\s]+([\d,]+)', content, re.IGNORECASE)
        if samples_match:
            report_data['total_samples'] = int(samples_match.group(1).replace(',', ''))
        
        # Extract application code percentage
        app_match = re.search(r'Application code[:\*\s]+[\d,]+\s+samples\s+\(([\d.]+)%\)', content)
        if app_match:
            report_data['app_pct'] = float(app_match.group(1))
            report_data['infra_pct'] = 100.0 - report_data['app_pct']
        
        # Extract hotspots
        hotspot_section = re.search(r'## OSPREY Application Hotspots\n\n(.*?)(?:##|$)', content, re.DOTALL)
        if hotspot_section:
            for match in re.finditer(r'\|\s*(\w+)\s*\|\s*([\d,]+)\s*\|\s*([\d.]+)%', hotspot_section.group(1)):
                category, samples, pct = match.groups()
                report_data['hotspots'][category] = {
                    'samples': int(samples.replace(',', '')),
                    'percentage': float(pct)
                }
        
        return report_data
    
    data['cpu'] = parse_report(cpu_report)
    data['allocation'] = parse_report(alloc_report)
    
    return data

def parse_perf_data(profile_dir: Path) -> Optional[Dict]:
    """Parse perf stat and report data."""
    perf_stat = profile_dir / "perf_stat.txt"
    perf_report = profile_dir / "perf_report.txt"
    
    if not perf_stat.exists() and not perf_report.exists():
        return None
    
    data = {
        'stat': {},
        'top_functions': []
    }
    
    # Parse perf stat
    if perf_stat.exists():
        with open(perf_stat, 'r') as f:
            content = f.read()
        
        # Extract key metrics
        patterns = {
            'cycles': r'(\d+(?:,\d+)*)\s+cycles',
            'instructions': r'(\d+(?:,\d+)*)\s+instructions',
            'cache_misses': r'(\d+(?:,\d+)*)\s+cache-misses',
            'cache_references': r'(\d+(?:,\d+)*)\s+cache-references',
            'branch_misses': r'(\d+(?:,\d+)*)\s+branch-misses',
            'page_faults': r'(\d+(?:,\d+)*)\s+page-faults',
        }
        
        for key, pattern in patterns.items():
            match = re.search(pattern, content)
            if match:
                value = match.group(1).replace(',', '')
                try:
                    data['stat'][key] = int(value)
                except ValueError:
                    pass
    
    # Parse perf report (top functions)
    if perf_report.exists():
        with open(perf_report, 'r') as f:
            lines = f.readlines()
        
        for line in lines:
            # Look for function entries: "  12.34%  function_name"
            match = re.match(r'\s+([\d.]+)%\s+(.+)', line)
            if match:
                percentage = float(match.group(1))
                func_name = match.group(2).strip()
                if percentage > 0.1:  # Only significant functions
                    data['top_functions'].append({
                        'function': func_name,
                        'percentage': percentage
                    })
        
        # Sort by percentage
        data['top_functions'].sort(key=lambda x: x['percentage'], reverse=True)
        data['top_functions'] = data['top_functions'][:20]  # Top 20
    
    return data if data['stat'] or data['top_functions'] else None

def generate_combined_report(output_dir: Path, test_name: str, gc_data: Optional[Dict], 
                            profiler_data: Dict, perf_data: Optional[Dict]) -> Path:
    """Generate combined analysis report."""
    report_file = output_dir / f"{test_name}_combined_analysis.md"
    
    with open(report_file, 'w') as f:
        f.write(f"# Combined Profile Analysis: {test_name}\n\n")
        f.write(f"Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n")
        
        # Executive Summary
        f.write("## Executive Summary\n\n")
        
        if gc_data:
            f.write(f"- **GC Overhead**: {gc_data['gc_overhead']:.2f}%\n")
            f.write(f"- **Total GC Events**: {gc_data['total_events']}\n")
            f.write(f"- **Max GC Pause**: {gc_data['max_pause']:.2f} ms\n")
            f.write(f"- **Peak Heap**: {gc_data['peak_heap']}M\n")
        
        if profiler_data.get('cpu'):
            cpu = profiler_data['cpu']
            f.write(f"- **CPU Samples**: {cpu['total_samples']:,}\n")
            f.write(f"- **Application Code**: {cpu['app_pct']:.1f}%\n")
        
        if profiler_data.get('allocation'):
            alloc = profiler_data['allocation']
            f.write(f"- **Allocation Samples**: {alloc['total_samples']:,}\n")
            f.write(f"- **Application Allocations**: {alloc['app_pct']:.1f}%\n")
        
        f.write("\n")
        
        # GC Analysis
        if gc_data:
            f.write("## GC Analysis\n\n")
            f.write("| Metric | Value |\n")
            f.write("|--------|-------|\n")
            f.write(f"| Total GC Events | {gc_data['total_events']} |\n")
            f.write(f"| Young GC | {gc_data['young_gc_count']} |\n")
            f.write(f"| Full GC | {gc_data['full_gc_count']} |\n")
            f.write(f"| Total Pause Time | {gc_data['total_pause_time']:.3f} s |\n")
            f.write(f"| Average Pause | {gc_data['average_pause']:.2f} ms |\n")
            f.write(f"| Maximum Pause | {gc_data['max_pause']:.2f} ms |\n")
            f.write(f"| GC Overhead | {gc_data['gc_overhead']:.2f}% |\n")
            f.write(f"| Peak Heap | {gc_data['peak_heap']}M |\n")
            f.write(f"| Total Duration | {gc_data['duration']:.2f} s |\n")
            f.write("\n")
        
        # CPU Profiling
        if profiler_data.get('cpu'):
            cpu = profiler_data['cpu']
            f.write("## CPU Profiling\n\n")
            f.write(f"- **Total Samples**: {cpu['total_samples']:,}\n")
            f.write(f"- **Application Code**: {cpu['app_pct']:.1f}%\n")
            f.write(f"- **Infrastructure**: {cpu['infra_pct']:.1f}%\n")
            
            if cpu['hotspots']:
                f.write("\n### Application Hotspots\n\n")
                f.write("| Category | Samples | Percentage |\n")
                f.write("|----------|---------|------------|\n")
                for cat, data in sorted(cpu['hotspots'].items(), key=lambda x: x[1]['percentage'], reverse=True):
                    f.write(f"| {cat} | {data['samples']:,} | {data['percentage']:.2f}% |\n")
            f.write("\n")
        
        # Allocation Profiling
        if profiler_data.get('allocation'):
            alloc = profiler_data['allocation']
            f.write("## Allocation Profiling\n\n")
            f.write(f"- **Total Samples**: {alloc['total_samples']:,}\n")
            f.write(f"- **Application Allocations**: {alloc['app_pct']:.1f}%\n")
            f.write(f"- **Infrastructure**: {alloc['infra_pct']:.1f}%\n")
            
            if alloc['hotspots']:
                f.write("\n### Allocation Hotspots\n\n")
                f.write("| Category | Samples | Percentage |\n")
                f.write("|----------|---------|------------|\n")
                for cat, data in sorted(alloc['hotspots'].items(), key=lambda x: x[1]['percentage'], reverse=True):
                    f.write(f"| {cat} | {data['samples']:,} | {data['percentage']:.2f}% |\n")
            f.write("\n")
        
        # Perf Data
        if perf_data:
            f.write("## Native Code Profiling (perf)\n\n")
            
            if perf_data['stat']:
                f.write("### Performance Statistics\n\n")
                f.write("| Metric | Value |\n")
                f.write("|--------|-------|\n")
                for key, value in perf_data['stat'].items():
                    f.write(f"| {key.replace('_', ' ').title()} | {value:,} |\n")
                f.write("\n")
            
            if perf_data['top_functions']:
                f.write("### Top Functions\n\n")
                f.write("| Function | Percentage |\n")
                f.write("|----------|------------|\n")
                for func in perf_data['top_functions'][:10]:
                    f.write(f"| `{func['function'][:60]}` | {func['percentage']:.2f}% |\n")
                f.write("\n")
        
        # Correlations and Insights
        f.write("## Correlations and Insights\n\n")
        
        insights = []
        
        if gc_data:
            if gc_data['gc_overhead'] < 1.0:
                insights.append("GC overhead is very low (< 1%) - memory is not a bottleneck")
            elif gc_data['gc_overhead'] < 5.0:
                insights.append("GC overhead is acceptable (< 5%) - memory optimization has low priority")
            else:
                insights.append("GC overhead is significant (> 5%) - memory optimization may be beneficial")
            
            if gc_data['full_gc_count'] == 0:
                insights.append("No Full GC events - heap sizing is appropriate")
            
            if gc_data['max_pause'] > 100:
                insights.append(f"Large GC pause detected ({gc_data['max_pause']:.0f}ms) - investigate system-level delays")
        
        if profiler_data.get('cpu') and profiler_data['cpu']['app_pct'] < 5.0:
            insights.append("Low application code visibility in CPU profile - test may be too short or infrastructure overhead dominates")
        
        if profiler_data.get('allocation') and profiler_data['allocation']['app_pct'] < 5.0:
            insights.append("Low application allocation visibility - test may be too short or class loading dominates")
        
        if perf_data and perf_data.get('stat'):
            stat = perf_data['stat']
            if 'cache_misses' in stat and 'cache_references' in stat:
                miss_rate = (stat['cache_misses'] / stat['cache_references'] * 100) if stat['cache_references'] > 0 else 0
                if miss_rate > 10:
                    insights.append(f"High cache miss rate ({miss_rate:.1f}%) - memory access patterns may need optimization")
        
        if insights:
            for insight in insights:
                f.write(f"- {insight}\n")
        else:
            f.write("No significant correlations identified.\n")
        
        f.write("\n")
        
        # Recommendations
        f.write("## Recommendations\n\n")
        
        recommendations = []
        
        if gc_data and gc_data['gc_overhead'] < 1.0:
            recommendations.append("**Memory**: GC overhead is very low - focus optimization on CPU-bound operations")
        
        if profiler_data.get('cpu') and profiler_data['cpu'].get('hotspots'):
            cpu_hotspots = profiler_data['cpu']['hotspots']
            if 'native_energy' in cpu_hotspots and cpu_hotspots['native_energy']['percentage'] > 5.0:
                recommendations.append("**CPU**: Native energy calculations are significant - profile C++ code directly")
            if 'astar' in cpu_hotspots and cpu_hotspots['astar']['percentage'] > 5.0:
                recommendations.append("**CPU**: A* search is significant - consider algorithm optimization or parallelization")
        
        if profiler_data.get('allocation') and profiler_data['allocation'].get('hotspots'):
            alloc_hotspots = profiler_data['allocation']['hotspots']
            if 'conformation' in alloc_hotspots and alloc_hotspots['conformation']['percentage'] > 5.0:
                recommendations.append("**Memory**: Conformation space operations allocate frequently - consider object pooling")
            if 'astar' in alloc_hotspots and alloc_hotspots['astar']['percentage'] > 5.0:
                recommendations.append("**Memory**: A* search trees allocate heavily - consider reducing allocation churn for tree nodes (approach TBD)")
        
        if not recommendations:
            recommendations.append("Continue profiling with longer-running tests to capture more application code")
            recommendations.append("Profile C++ native code directly to understand energy calculation bottlenecks")
        
        for rec in recommendations:
            f.write(f"- {rec}\n")
        
        f.write("\n")
    
    return report_file

def main():
    parser = argparse.ArgumentParser(description="Combine analysis from GC logs, CPU profiles, and allocation profiles")
    parser.add_argument("--output-dir", type=Path, required=True, help="Directory containing profiling results")
    parser.add_argument("--test-name", type=str, help="Test case name (auto-detected if not provided)")
    
    args = parser.parse_args()
    
    output_dir = args.output_dir
    
    # Auto-detect test name from directory
    if not args.test_name:
        test_name = output_dir.name
    else:
        test_name = args.test_name
    
    print(f"Analyzing combined profiles for: {test_name}")
    print(f"Output directory: {output_dir}")
    print("")
    
    # Parse GC analysis
    gc_analysis_dir = output_dir / "gc_analysis"
    gc_data = None
    if gc_analysis_dir.exists():
        print("Parsing GC analysis...")
        gc_data = parse_gc_analysis(gc_analysis_dir)
        if gc_data:
            print(f"  Found GC data: {gc_data['total_events']} events, {gc_data['gc_overhead']:.2f}% overhead")
        else:
            print("  No GC data found")
    else:
        print("  GC analysis directory not found")
    
    print("")
    
    # Parse async-profiler analysis
    print("Parsing async-profiler analysis...")
    profiler_data = parse_async_profiler_analysis(output_dir)
    if profiler_data.get('cpu'):
        print(f"  Found CPU profile: {profiler_data['cpu']['total_samples']:,} samples")
    else:
        print("  No CPU profile found")
    
    if profiler_data.get('allocation'):
        print(f"  Found allocation profile: {profiler_data['allocation']['total_samples']:,} samples")
    else:
        print("  No allocation profile found")
    
    print("")
    
    # Parse perf data
    print("Parsing perf data...")
    perf_data = parse_perf_data(output_dir)
    if perf_data:
        if perf_data['stat']:
            print(f"  Found perf stat data: {len(perf_data['stat'])} metrics")
        if perf_data['top_functions']:
            print(f"  Found perf report: {len(perf_data['top_functions'])} top functions")
    else:
        print("  No perf data found")
    
    print("")
    
    # Generate combined report
    print("Generating combined analysis report...")
    report_file = generate_combined_report(output_dir, test_name, gc_data, profiler_data, perf_data)
    print(f"Report generated: {report_file}")
    
    # Summary
    print("")
    print("=== Summary ===")
    if gc_data:
        print(f"GC: {gc_data['gc_overhead']:.2f}% overhead, {gc_data['total_events']} events")
    if profiler_data.get('cpu'):
        print(f"CPU: {profiler_data['cpu']['app_pct']:.1f}% application code")
    if profiler_data.get('allocation'):
        print(f"Allocation: {profiler_data['allocation']['app_pct']:.1f}% application allocations")
    if perf_data:
        print("Perf: Native code profiling data available")

if __name__ == '__main__':
    main()

