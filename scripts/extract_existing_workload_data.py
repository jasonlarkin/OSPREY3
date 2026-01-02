#!/usr/bin/env python3
"""
Extract workload variation data from existing profiling results.
Synthesizes data from multiple sources to build scaling relationships.
"""
import sys
import json
import re
from pathlib import Path
from collections import defaultdict

def extract_from_full_kstar(test_dir):
    """Extract data from full_kstar profiling."""
    results = {}
    
    test_path = Path(test_dir)
    if not test_path.exists():
        return results
    
    # Look for analysis files
    for analysis_file in test_path.glob("*analysis*.md"):
        with open(analysis_file) as f:
            content = f.read()
            
            # Extract pair counts
            pairs_match = re.search(r'(\d+)\s+pairs', content, re.IGNORECASE)
            if pairs_match:
                results['pair_count'] = int(pairs_match.group(1))
            
            # Extract duration
            time_match = re.search(r'Duration[:\s]+([\d.]+)\s*(?:s|seconds)', content, re.IGNORECASE)
            if time_match:
                results['duration_seconds'] = float(time_match.group(1))
            
            # Extract GC overhead
            gc_match = re.search(r'GC\s+Overhead[:\s]+([\d.]+)%', content, re.IGNORECASE)
            if gc_match:
                results['gc_overhead_percent'] = float(gc_match.group(1))
    
    # Look for GC logs
    for gc_log in test_path.glob("*.log"):
        if 'gc' in gc_log.name.lower():
            # Parse GC log
            stats = parse_gc_log(gc_log)
            if stats:
                results.update(stats)
    
    return results

def parse_gc_log(gc_log_path):
    """Parse GC log file."""
    stats = {}
    
    with open(gc_log_path) as f:
        content = f.read()
        
        # Count GC events
        young_gcs = len(re.findall(r'GC\(\d+\)\s+Young', content))
        full_gcs = len(re.findall(r'GC\(\d+\)\s+Full', content))
        
        if young_gcs or full_gcs:
            stats['young_gc_count'] = young_gcs
            stats['full_gc_count'] = full_gcs
            stats['total_gc_count'] = young_gcs + full_gcs
            
            # Extract pause times
            pauses = re.findall(r'([\d.]+)ms', content)
            if pauses:
                pause_times = [float(p) for p in pauses]
                stats['total_pause_ms'] = sum(pause_times)
                stats['avg_pause_ms'] = sum(pause_times) / len(pause_times)
                stats['max_pause_ms'] = max(pause_times)
            
            # Extract memory
            memory_matches = re.findall(r'(\d+)M->(\d+)M\((\d+)M\)', content)
            if memory_matches:
                peak = max(int(m[2]) for m in memory_matches)
                stats['peak_memory_mb'] = peak
    
    return stats

def extract_from_intermediate_results(results_file):
    """Extract from intermediate_results.md."""
    results = {}
    
    if not Path(results_file).exists():
        return results
    
    with open(results_file) as f:
        content = f.read()
        
        # Extract energy matrix times
        emat_matches = re.findall(r'(\w+)\s+Component.*?Entries[:\s]+(\d+).*?Time[:\s]+([\d.]+)\s*s', content, re.DOTALL)
        if emat_matches:
            results['energy_matrices'] = {}
            for component, entries, time in emat_matches:
                results['energy_matrices'][component.lower()] = {
                    'entries': int(entries),
                    'time_seconds': float(time)
                }
    
    return results

def main():
    repo_root = Path(__file__).parent.parent
    pipeline_analysis = repo_root / "pipeline_analysis"
    output_dir = pipeline_analysis / "kstar" / "workload_variation"
    output_dir.mkdir(parents=True, exist_ok=True)
    
    all_results = {}
    
    # Extract from full_kstar profiling
    full_kstar_dir = pipeline_analysis / "full_kstar"
    if full_kstar_dir.exists():
        for test_dir in full_kstar_dir.iterdir():
            if test_dir.is_dir() and test_dir.name.startswith('test'):
                test_name = test_dir.name
                data = extract_from_full_kstar(test_dir)
                if data:
                    all_results[test_name] = data
                    print(f"Extracted data from {test_name}")
    
    # Extract from intermediate results
    intermediate_file = output_dir / "intermediate_results.md"
    if intermediate_file.exists():
        intermediate_data = extract_from_intermediate_results(intermediate_file)
        if intermediate_data:
            all_results['intermediate'] = intermediate_data
            print(f"Extracted data from intermediate results")
    
    # Add known data from PROBLEM_SIZE_COVERAGE.md
    known_data = {
        'test2RL0': {
            'complex_pairs': 16734,
            'ligand_pairs': 7575,
            'protein_pairs': 1008,
            'duration_seconds': 169.6,
            'gc_overhead_percent': 0.51
        },
        'test1GUA11': {
            'duration_seconds': 79.6,
            'gc_overhead_percent': 3.40
        }
    }
    
    # Merge known data
    for test_name, data in known_data.items():
        if test_name not in all_results:
            all_results[test_name] = {}
        all_results[test_name].update(data)
    
    # Write results
    results_file = output_dir / "extracted_workload_data.json"
    with open(results_file, 'w') as f:
        json.dump(all_results, f, indent=2)
    
    # Generate summary
    summary_file = output_dir / "workload_scaling_summary.md"
    with open(summary_file, 'w') as f:
        f.write("# K* Workload Scaling Summary\n\n")
        f.write("## Extracted Data\n\n")
        
        for test_name, data in all_results.items():
            f.write(f"### {test_name}\n\n")
            for key, value in data.items():
                f.write(f"- **{key}**: {value}\n")
            f.write("\n")
        
        # Calculate scaling if we have multiple data points
        if 'test2RL0' in all_results:
            data = all_results['test2RL0']
            if 'complex_pairs' in data and 'protein_pairs' in data:
                f.write("## Scaling Relationships (2RL0)\n\n")
                
                complex_pairs = data['complex_pairs']
                ligand_pairs = data['ligand_pairs']
                protein_pairs = data['protein_pairs']
                
                f.write(f"| Component | Pairs | Ratio vs Protein |\n")
                f.write(f"|-----------|-------|------------------|\n")
                f.write(f"| Protein | {protein_pairs} | 1.0x |\n")
                f.write(f"| Ligand | {ligand_pairs} | {ligand_pairs/protein_pairs:.2f}x |\n")
                f.write(f"| Complex | {complex_pairs} | {complex_pairs/protein_pairs:.2f}x |\n")
                
                f.write("\n### Energy Matrix Scaling (from intermediate results)\n\n")
                if 'intermediate' in all_results and 'energy_matrices' in all_results['intermediate']:
                    emats = all_results['intermediate']['energy_matrices']
                    f.write("| Component | Entries | Time (s) | Time/Entry (ms) |\n")
                    f.write("|-----------|---------|----------|-----------------|\n")
                    for comp, emat_data in emats.items():
                        entries = emat_data['entries']
                        time = emat_data['time_seconds']
                        time_per_entry = (time * 1000) / entries
                        f.write(f"| {comp} | {entries} | {time:.2f} | {time_per_entry:.3f} |\n")
    
    print(f"\nResults saved to: {results_file}")
    print(f"Summary saved to: {summary_file}")

if __name__ == '__main__':
    main()

