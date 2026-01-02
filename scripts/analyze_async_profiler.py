#!/usr/bin/env python3
"""
Analyze async-profiler HTML flame graphs to identify hotspots.
Extracts function call data, identifies top CPU/memory consumers,
and generates reports for optimization opportunities.
"""

import argparse
import re
import sys
import json
from pathlib import Path
from typing import Dict, List, Tuple, Optional
from collections import defaultdict
import html

def parse_async_profiler_html(html_file: Path) -> Dict:
    """Parse async-profiler HTML file and extract flame graph data."""
    with open(html_file, 'r', encoding='utf-8') as f:
        content = f.read()
    
    # Extract the f() function calls that define the flame graph
    # Format: f(level, left, width, type, 'title'[, inln, c1, int])
    # The last 3 parameters are optional
    pattern = r'f\((\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*[\'"]([^\'"]+)[\'"](?:,\s*([^,)]+)(?:,\s*([^,)]+)(?:,\s*([^)]+))?)?)?\)'
    
    frames = []
    for match in re.finditer(pattern, content):
        groups = match.groups()
        level, left, width, ftype, title = groups[0:5]
        inln = groups[5] if groups[5] else ''
        c1 = groups[6] if groups[6] else ''
        int_val = groups[7] if groups[7] else ''
        
        frames.append({
            'level': int(level),
            'left': int(left),
            'width': int(width),
            'type': int(ftype),
            'title': html.unescape(title),
            'inln': inln.strip(),
            'c1': c1.strip(),
            'int': int_val.strip()
        })
    
    # Root width is the width of the level 0 frame (usually the first one)
    root_width = 0
    for frame in frames:
        if frame['level'] == 0:
            root_width = max(root_width, frame['left'] + frame['width'])
    
    # If no level 0 frame found, use max width
    if root_width == 0 and frames:
        root_width = max((f['left'] + f['width'] for f in frames), default=0)
    
    return {
        'frames': frames,
        'root_width': root_width,
        'total_samples': root_width
    }

def build_call_tree(frames: List[Dict]) -> Dict:
    """Build hierarchical call tree from flat frame list."""
    # Sort by level and left position
    frames_sorted = sorted(frames, key=lambda x: (x['level'], x['left']))
    
    # Build tree structure
    tree = {
        'name': 'all',
        'width': max((f['left'] + f['width'] for f in frames), default=0),
        'children': [],
        'self_time': 0
    }
    
    # Group frames by level
    by_level = defaultdict(list)
    for frame in frames_sorted:
        by_level[frame['level']].append(frame)
    
    # Build parent-child relationships
    def find_parent(frame, parent_level, parent_left, parent_right):
        """Find parent frame for a given frame."""
        if parent_level < 0:
            return tree
        
        # Look for parent at previous level that contains this frame
        for parent_frame in by_level.get(parent_level, []):
            p_left = parent_frame['left']
            p_right = p_left + parent_frame['width']
            if p_left <= frame['left'] and frame['left'] + frame['width'] <= p_right:
                return parent_frame
        
        return None
    
    # Build tree structure
    node_map = {'0': tree}
    
    for level in sorted(by_level.keys()):
        if level == 0:
            continue
        
        for frame in by_level[level]:
            parent_level = level - 1
            parent = None
            
            # Find parent
            for parent_frame in by_level.get(parent_level, []):
                p_left = parent_frame['left']
                p_right = p_left + parent_frame['width']
                if p_left <= frame['left'] and frame['left'] + frame['width'] <= p_right:
                    parent = parent_frame
                    break
            
            if parent:
                # Create node
                node = {
                    'name': frame['title'],
                    'width': frame['width'],
                    'children': [],
                    'self_time': frame['width'],
                    'level': level
                }
                
                # Store in map for lookup
                node_key = f"{level}_{frame['left']}"
                node_map[node_key] = node
                
                # Find parent node in tree
                parent_key = f"{parent_level}_{parent['left']}"
                if parent_key in node_map:
                    node_map[parent_key]['children'].append(node)
                elif level == 1:
                    tree['children'].append(node)
    
    return tree

def is_infrastructure(func_name: str) -> bool:
    """Check if function is infrastructure (Gradle, JVM, etc.) not application code."""
    infrastructure_patterns = [
        r'org/gradle',
        r'groovy',
        r'java/lang/reflect',
        r'jdk/internal',
        r'java/util/concurrent',
        r'java/util/stream',
        r'java/util/Optional',
        r'java/lang/Thread',
        r'java/lang/ClassLoader',
        r'CompileBroker',
        r'Compile::',
        r'C2Compiler',
        r'PhaseCFG',
        r'CodeHeap',
        r'G1ServiceThread',
        r'KlassFactory',
        r'ClassFileParser',
        r'Metaspace',
        r'start_thread',
        r'thread_native_entry',
        r'JavaThread',
        r'Thread::',
        r'__malloc',
        r'_int_malloc',
        r'vframe::',
        r'compiledVFrame',
    ]
    for pattern in infrastructure_patterns:
        if re.search(pattern, func_name, re.IGNORECASE):
            return True
    return False

def extract_top_functions(frames: List[Dict], total_samples: int, top_n: int = 20, filter_infrastructure: bool = True) -> List[Tuple[str, int, float]]:
    """Extract top functions by sample count."""
    func_samples = defaultdict(int)
    
    for frame in frames:
        func_name = frame['title']
        # Filter out infrastructure if requested
        if filter_infrastructure and is_infrastructure(func_name):
            continue
        func_samples[func_name] += frame['width']
    
    # Sort by sample count
    sorted_funcs = sorted(func_samples.items(), key=lambda x: x[1], reverse=True)
    
    results = []
    for func_name, samples in sorted_funcs[:top_n]:
        percentage = (samples / total_samples * 100) if total_samples > 0 else 0
        results.append((func_name, samples, percentage))
    
    return results

def identify_osprey_hotspots(frames: List[Dict], total_samples: int) -> Dict:
    """Identify OSPREY-specific hotspots (application code only)."""
    osprey_patterns = {
        'energy_calc': r'(edu\.duke\.cs\.osprey.*energy|ConfEnergy|calcEnergy|minimize|EnergyMatrix)',
        'astar': r'(edu\.duke\.cs\.osprey.*AStar|edu\.duke\.cs\.osprey.*astar|edu\.duke\.cs\.osprey.*search)',
        'partition': r'(edu\.duke\.cs\.osprey.*Partition|PartitionFunction)',
        'kstar': r'(edu\.duke\.cs\.osprey.*KStar|edu\.duke\.cs\.osprey.*kstar)',
        'conformation': r'(edu\.duke\.cs\.osprey.*Conf|edu\.duke\.cs\.osprey.*conformation|SimpleConfSpace)',
        'matrix': r'(edu\.duke\.cs\.osprey.*Matrix|EnergyMatrix)',
        'native_energy': r'(ConfEcalc|native|JNI)',
    }
    
    hotspots = defaultdict(lambda: {'samples': 0, 'functions': [], 'unique_functions': set()})
    
    func_samples = defaultdict(int)
    for frame in frames:
        func_name = frame['title']
        # Skip infrastructure
        if is_infrastructure(func_name):
            continue
        func_samples[func_name] += frame['width']
    
    for func_name, samples in func_samples.items():
        for category, pattern in osprey_patterns.items():
            if re.search(pattern, func_name, re.IGNORECASE):
                hotspots[category]['samples'] += samples
                hotspots[category]['unique_functions'].add(func_name)
    
    # Convert sets to lists and calculate percentages
    for category in hotspots:
        hotspots[category]['functions'] = list(hotspots[category]['unique_functions'])
        hotspots[category]['percentage'] = (hotspots[category]['samples'] / total_samples * 100) if total_samples > 0 else 0
        del hotspots[category]['unique_functions']
    
    return hotspots

def generate_report(profile_data: Dict, profile_type: str, output_dir: Path):
    """Generate analysis report."""
    frames = profile_data['frames']
    total_samples = profile_data['total_samples']
    
    # Extract top functions (application code only)
    top_functions = extract_top_functions(frames, total_samples, top_n=30, filter_infrastructure=True)
    
    # Also get top functions including infrastructure for comparison
    top_all = extract_top_functions(frames, total_samples, top_n=10, filter_infrastructure=False)
    
    # Identify OSPREY hotspots
    hotspots = identify_osprey_hotspots(frames, total_samples)
    
    # Calculate infrastructure vs application ratio
    infra_samples = sum(f['width'] for f in frames if is_infrastructure(f['title']))
    app_samples = total_samples - infra_samples
    infra_pct = (infra_samples / total_samples * 100) if total_samples > 0 else 0
    app_pct = (app_samples / total_samples * 100) if total_samples > 0 else 0
    
    # Generate report
    report_file = output_dir / f"{profile_type}_analysis.md"
    
    with open(report_file, 'w') as f:
        f.write(f"# {profile_type.upper()} Profile Analysis\n\n")
        f.write(f"Total samples: {total_samples:,}\n\n")
        
        f.write("## Infrastructure vs Application Code\n\n")
        f.write(f"- Infrastructure (Gradle/JVM): {infra_samples:,} samples ({infra_pct:.1f}%)\n")
        f.write(f"- Application code: {app_samples:,} samples ({app_pct:.1f}%)\n\n")
        
        if top_functions:
            f.write("## Top Application Functions\n\n")
            f.write("| Function | Samples | Percentage |\n")
            f.write("|----------|---------|------------|\n")
            for func_name, samples, pct in top_functions:
                f.write(f"| `{func_name[:80]}` | {samples:,} | {pct:.2f}% |\n")
        else:
            f.write("## Top Application Functions\n\n")
            f.write("No application code detected (all samples are infrastructure).\n")
            f.write("\nTop functions (including infrastructure):\n\n")
            f.write("| Function | Samples | Percentage |\n")
            f.write("|----------|---------|------------|\n")
            for func_name, samples, pct in top_all[:10]:
                f.write(f"| `{func_name[:80]}` | {samples:,} | {pct:.2f}% |\n")
        
        f.write("\n## OSPREY Application Hotspots\n\n")
        if any(hotspots.values()):
            f.write("| Category | Samples | Percentage | Function Count |\n")
            f.write("|----------|---------|------------|----------------|\n")
            for category, data in sorted(hotspots.items(), key=lambda x: x[1]['samples'], reverse=True):
                if data['samples'] > 0:
                    f.write(f"| {category} | {data['samples']:,} | {data['percentage']:.2f}% | {len(data['functions'])} functions |\n")
            
            f.write("\n### Detailed Function Lists\n\n")
            for category, data in sorted(hotspots.items(), key=lambda x: x[1]['samples'], reverse=True):
                if data['samples'] > 0 and data['functions']:
                    f.write(f"**{category}** ({data['percentage']:.2f}% of total):\n")
                    for func in sorted(data['functions'])[:10]:
                        f.write(f"- `{func}`\n")
                    if len(data['functions']) > 10:
                        f.write(f"- ... ({len(data['functions']) - 10} more)\n")
                    f.write("\n")
        else:
            f.write("No OSPREY application code detected in profile.\n")
            f.write("This may indicate:\n")
            f.write("- Test execution is too fast to capture\n")
            f.write("- Profiling captured mostly setup/teardown\n")
            f.write("- Application code is in native/C++ components\n\n")
        
        # Workload characteristics
        f.write("## Workload Characteristics\n\n")
        if profile_type == 'allocation':
            f.write("### Memory Allocation Patterns\n\n")
            if hotspots.get('energy_calc', {}).get('samples', 0) > 0:
                f.write("- Energy calculations allocate memory\n")
            if hotspots.get('astar', {}).get('samples', 0) > 0:
                f.write("- A* search tree operations allocate memory\n")
            if hotspots.get('partition', {}).get('samples', 0) > 0:
                f.write("- Partition function computations allocate memory\n")
            if hotspots.get('conformation', {}).get('samples', 0) > 0:
                f.write("- Conformation space operations allocate memory\n")
        else:
            f.write("### CPU Processing Patterns\n\n")
            if hotspots.get('energy_calc', {}).get('samples', 0) > 0:
                f.write("- Energy calculations are CPU-intensive\n")
            if hotspots.get('astar', {}).get('samples', 0) > 0:
                f.write("- A* search tree traversal is CPU-intensive\n")
            if hotspots.get('partition', {}).get('samples', 0) > 0:
                f.write("- Partition function computations are CPU-intensive\n")
            if hotspots.get('native_energy', {}).get('samples', 0) > 0:
                f.write("- Native energy calculations (C++) are CPU-intensive\n")
    
    print(f"Report generated: {report_file}")

def main():
    parser = argparse.ArgumentParser(description="Analyze async-profiler HTML profiles")
    parser.add_argument("--profile-dir", type=Path, required=True, help="Directory containing profile HTML files")
    parser.add_argument("--output-dir", type=Path, help="Output directory for reports (default: profile-dir)")
    
    args = parser.parse_args()
    
    if not args.profile_dir.exists():
        print(f"Error: Profile directory not found: {args.profile_dir}")
        sys.exit(1)
    
    output_dir = args.output_dir or args.profile_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Find all HTML profile files (recursively)
    html_files = list(args.profile_dir.rglob("*.html"))
    
    if not html_files:
        print(f"No HTML profile files found in {args.profile_dir}")
        sys.exit(1)
    
    print(f"Found {len(html_files)} profile file(s)")
    
    for html_file in html_files:
        # Get relative path from profile_dir to preserve subdirectory structure
        rel_path = html_file.relative_to(args.profile_dir)
        print(f"\nAnalyzing: {rel_path}")
        
        # Determine profile type
        if 'cpu' in html_file.name.lower():
            profile_type = 'cpu'
        elif 'alloc' in html_file.name.lower():
            profile_type = 'allocation'
        else:
            profile_type = 'unknown'
        
        # Use subdirectory name + profile type for report name
        # e.g., test2RL0/cpu_profile.html -> test2RL0_cpu
        report_name = f"{rel_path.parent.name}_{profile_type}" if rel_path.parent != Path('.') else f"{html_file.stem}_{profile_type}"
        
        try:
            profile_data = parse_async_profiler_html(html_file)
            print(f"  Parsed {len(profile_data['frames'])} frames, {profile_data['total_samples']:,} total samples")
            
            generate_report(profile_data, report_name, output_dir)
            
        except Exception as e:
            print(f"  Error analyzing {html_file.name}: {e}")
            import traceback
            traceback.print_exc()
    
    print(f"\nAnalysis complete. Reports in: {output_dir}")

if __name__ == '__main__':
    main()

