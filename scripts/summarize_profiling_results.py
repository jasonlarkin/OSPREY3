#!/usr/bin/env python3
"""
Summarize profiling results across all test cases.
Provides overall workload characteristics and processing requirements.
"""

import re
from pathlib import Path
from typing import Dict, List
from collections import defaultdict

def parse_markdown_report(report_file: Path) -> Dict:
    """Parse a markdown analysis report."""
    data = {
        'total_samples': 0,
        'infra_pct': 0,
        'app_pct': 0,
        'hotspots': {},
        'top_functions': []
    }
    
    with open(report_file, 'r') as f:
        content = f.read()
    
    # Extract total samples
    match = re.search(r'Total samples: ([\d,]+)', content)
    if match:
        data['total_samples'] = int(match.group(1).replace(',', ''))
    
    # Extract infrastructure vs application
    match = re.search(r'Application code: [\d,]+\s+samples\s+\(([\d.]+)%\)', content)
    if match:
        data['app_pct'] = float(match.group(1))
        data['infra_pct'] = 100.0 - data['app_pct']
    
    # Extract hotspots
    hotspot_section = re.search(r'## OSPREY Application Hotspots\n\n(.*?)(?:##|$)', content, re.DOTALL)
    if hotspot_section:
        for match in re.finditer(r'\|\s*(\w+)\s*\|\s*([\d,]+)\s*\|\s*([\d.]+)%', hotspot_section.group(1)):
            category, samples, pct = match.groups()
            data['hotspots'][category] = {
                'samples': int(samples.replace(',', '')),
                'percentage': float(pct)
            }
    
    return data

def main():
    profile_dir = Path("pipeline_analysis/cpu")
    
    if not profile_dir.exists():
        print(f"Error: Profile directory not found: {profile_dir}")
        return
    
    # Find all analysis reports
    reports = list(profile_dir.glob("*_analysis.md"))
    
    if not reports:
        print("No analysis reports found. Run analyze_async_profiler.py first.")
        return
    
    # Group by test case and type
    test_cases = defaultdict(dict)
    
    for report in reports:
        # Parse test case name and type from filename
        # e.g., test2RL0_cpu_analysis.md -> test2RL0, cpu
        match = re.match(r'(.+?)_(cpu|allocation)_analysis\.md', report.name)
        if match:
            test_name, profile_type = match.groups()
            data = parse_markdown_report(report)
            test_cases[test_name][profile_type] = data
    
    # Generate summary
    summary_file = profile_dir / "profiling_summary.md"
    
    with open(summary_file, 'w') as f:
        f.write("# Profiling Results Summary\n\n")
        f.write("## Test Cases Analyzed\n\n")
        
        for test_name in sorted(test_cases.keys()):
            f.write(f"### {test_name}\n\n")
            case_data = test_cases[test_name]
            
            if 'cpu' in case_data:
                cpu_data = case_data['cpu']
                f.write(f"**CPU Profile:**\n")
                f.write(f"- Total samples: {cpu_data['total_samples']:,}\n")
                f.write(f"- Application code: {cpu_data['app_pct']:.1f}%\n")
                f.write(f"- Infrastructure: {cpu_data['infra_pct']:.1f}%\n")
                if cpu_data['hotspots']:
                    f.write("- Hotspots:\n")
                    for cat, data in sorted(cpu_data['hotspots'].items(), key=lambda x: x[1]['percentage'], reverse=True):
                        f.write(f"  - {cat}: {data['percentage']:.2f}%\n")
                f.write("\n")
            
            if 'allocation' in case_data:
                alloc_data = case_data['allocation']
                f.write(f"**Allocation Profile:**\n")
                f.write(f"- Total samples: {alloc_data['total_samples']:,}\n")
                f.write(f"- Application code: {alloc_data['app_pct']:.1f}%\n")
                f.write(f"- Infrastructure: {alloc_data['infra_pct']:.1f}%\n")
                if alloc_data['hotspots']:
                    f.write("- Hotspots:\n")
                    for cat, data in sorted(alloc_data['hotspots'].items(), key=lambda x: x[1]['percentage'], reverse=True):
                        f.write(f"  - {cat}: {data['percentage']:.2f}%\n")
                f.write("\n")
        
        # Cross-case analysis
        f.write("## Cross-Case Analysis\n\n")
        
        # Aggregate hotspots across all cases
        all_cpu_hotspots = defaultdict(lambda: {'total_samples': 0, 'cases': []})
        all_alloc_hotspots = defaultdict(lambda: {'total_samples': 0, 'cases': []})
        
        for test_name, case_data in test_cases.items():
            if 'cpu' in case_data:
                for cat, data in case_data['cpu']['hotspots'].items():
                    all_cpu_hotspots[cat]['total_samples'] += data['samples']
                    all_cpu_hotspots[cat]['cases'].append(test_name)
            
            if 'allocation' in case_data:
                for cat, data in case_data['allocation']['hotspots'].items():
                    all_alloc_hotspots[cat]['total_samples'] += data['samples']
                    all_alloc_hotspots[cat]['cases'].append(test_name)
        
        if all_cpu_hotspots:
            f.write("### CPU Processing Patterns (Across All Cases)\n\n")
            f.write("| Category | Total Samples | Cases |\n")
            f.write("|----------|---------------|-------|\n")
            for cat, data in sorted(all_cpu_hotspots.items(), key=lambda x: x[1]['total_samples'], reverse=True):
                cases_str = ', '.join(set(data['cases']))
                f.write(f"| {cat} | {data['total_samples']:,} | {cases_str} |\n")
            f.write("\n")
        
        if all_alloc_hotspots:
            f.write("### Memory Allocation Patterns (Across All Cases)\n\n")
            f.write("| Category | Total Samples | Cases |\n")
            f.write("|----------|---------------|-------|\n")
            for cat, data in sorted(all_alloc_hotspots.items(), key=lambda x: x[1]['total_samples'], reverse=True):
                cases_str = ', '.join(set(data['cases']))
                f.write(f"| {cat} | {data['total_samples']:,} | {cases_str} |\n")
            f.write("\n")
        
        # Workload characteristics
        f.write("## Workload Characteristics\n\n")
        
        total_cpu_samples = sum(case_data.get('cpu', {}).get('total_samples', 0) for case_data in test_cases.values())
        total_alloc_samples = sum(case_data.get('allocation', {}).get('total_samples', 0) for case_data in test_cases.values())
        
        f.write(f"### Overall Statistics\n\n")
        f.write(f"- Total CPU samples: {total_cpu_samples:,}\n")
        f.write(f"- Total allocation samples: {total_alloc_samples:,}\n")
        f.write(f"- Test cases: {len(test_cases)}\n\n")
        
        # Key findings
        f.write("### Key Findings\n\n")
        
        # Check if we're seeing actual OSPREY code
        has_osprey_code = any(
            case_data.get('cpu', {}).get('app_pct', 0) > 5.0 or
            case_data.get('allocation', {}).get('app_pct', 0) > 5.0
            for case_data in test_cases.values()
        )
        
        if not has_osprey_code:
            f.write("**Warning:** Profiles show mostly infrastructure (Gradle/JVM) overhead.\n")
            f.write("This suggests:\n")
            f.write("- Test execution may be too fast to capture application code\n")
            f.write("- Profiling may be capturing setup/teardown rather than computation\n")
            f.write("- Application code may be in native/C++ components not visible to JVM profiler\n\n")
        else:
            f.write("**Application code detected** in profiles.\n\n")
        
        # Processing requirements
        f.write("### Processing Requirements\n\n")
        if all_cpu_hotspots:
            f.write("**CPU-Intensive Operations:**\n")
            for cat, data in sorted(all_cpu_hotspots.items(), key=lambda x: x[1]['total_samples'], reverse=True)[:5]:
                f.write(f"- {cat}: {data['total_samples']:,} samples across {len(set(data['cases']))} case(s)\n")
            f.write("\n")
        
        if all_alloc_hotspots:
            f.write("**Memory-Intensive Operations:**\n")
            for cat, data in sorted(all_alloc_hotspots.items(), key=lambda x: x[1]['total_samples'], reverse=True)[:5]:
                f.write(f"- {cat}: {data['total_samples']:,} samples across {len(set(data['cases']))} case(s)\n")
            f.write("\n")
        
        # Recommendations
        f.write("### Recommendations\n\n")
        f.write("Based on profiling data:\n\n")
        
        if all_cpu_hotspots.get('native_energy', {}).get('total_samples', 0) > 0:
            f.write("- **Native energy calculations** are CPU-intensive - consider SIMD/GPU acceleration\n")
        
        if all_cpu_hotspots.get('astar', {}).get('total_samples', 0) > 0:
            f.write("- **A* search** is CPU-intensive - consider parallel search or optimized data structures\n")
        
        if all_alloc_hotspots.get('conformation', {}).get('total_samples', 0) > 0:
            f.write("- **Conformation space operations** allocate frequently - consider object pooling or other allocation-churn reduction\n")
        
        if all_alloc_hotspots.get('astar', {}).get('total_samples', 0) > 0:
            f.write("- **A* search trees** allocate heavily - consider allocation-churn reduction for tree nodes (approach TBD)\n")
        
        if all_alloc_hotspots.get('partition', {}).get('total_samples', 0) > 0:
            f.write("- **Partition functions** allocate heavily - consider allocation-churn reduction for computation objects (approach TBD)\n")
    
    print(f"Summary generated: {summary_file}")

if __name__ == '__main__':
    main()

