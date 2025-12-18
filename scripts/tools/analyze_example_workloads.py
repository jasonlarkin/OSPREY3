#!/usr/bin/env python3
"""
Analyze OSPREY example workloads to determine:
- Number of atoms (from PDB files)
- Distribution of workload sizes
- Which examples are suitable for benchmarking

This script can analyze by inspection (PDB files) and optionally by execution.
"""

import os
import sys
import re
from pathlib import Path
from collections import defaultdict
from typing import List, Dict, Tuple, Optional
import matplotlib.pyplot as plt
import numpy as np

def count_atoms_pdb(pdb_path: Path) -> Optional[int]:
    """Count atoms in a PDB file (ATOM and HETATM records)."""
    try:
        count = 0
        with open(pdb_path, 'r') as f:
            for line in f:
                if line.startswith(('ATOM  ', 'HETATM')):
                    count += 1
        return count if count > 0 else None
    except Exception as e:
        return None

def parse_system_cfg(cfg_path: Path) -> Dict:
    """Parse System.cfg file to extract basic info."""
    info = {}
    try:
        with open(cfg_path, 'r') as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith('#'):
                    continue
                if ':' in line:
                    key, value = line.split(':', 1)
                    key = key.strip().lower()
                    value = value.strip()
                    if key == 'pdbname':
                        info['pdb_file'] = value
                    elif key == 'numofstrands':
                        info['num_strands'] = int(value)
                    elif key.startswith('strand'):
                        if 'strands' not in info:
                            info['strands'] = []
                        # Parse strand range like "2 73"
                        parts = value.split()
                        if len(parts) >= 2:
                            try:
                                start, end = int(parts[0]), int(parts[1])
                                info['strands'].append((start, end))
                            except ValueError:
                                pass
    except Exception as e:
        pass
    return info

def analyze_example_dir(example_dir: Path) -> Dict:
    """Analyze a single example directory."""
    info = {
        'name': example_dir.name,
        'pdb_files': [],
        'config_files': [],
        'atom_counts': [],
        'type': 'unknown'
    }
    
    # Determine type
    name = example_dir.name.lower()
    if 'junit' in name:
        info['type'] = 'junit'
    elif 'python' in name:
        info['type'] = 'python'
    elif 'gpu' in name:
        info['type'] = 'gpu'
    elif name.startswith('1cc8'):
        info['type'] = '1CC8'
    elif name.startswith('2rl0'):
        info['type'] = '2RL0'
    elif name.startswith('1dg9'):
        info['type'] = '1DG9'
    elif name.startswith('1fsv'):
        info['type'] = '1FSV'
    elif name.startswith('4hem'):
        info['type'] = '4HEM'
    elif name.startswith('4npd'):
        info['type'] = '4NPD'
    else:
        info['type'] = 'other'
    
    # Find PDB files
    for pdb_file in example_dir.rglob('*.pdb'):
        info['pdb_files'].append(str(pdb_file.relative_to(example_dir)))
        atom_count = count_atoms_pdb(pdb_file)
        if atom_count:
            info['atom_counts'].append(atom_count)
    
    # Find config files
    for cfg_file in example_dir.rglob('*.cfg'):
        info['config_files'].append(str(cfg_file.relative_to(example_dir)))
    
    # Try to parse System.cfg
    system_cfg = example_dir / 'System.cfg'
    if not system_cfg.exists():
        # Try other common names
        for name in ['cfgSystem.txt', 'System.cfg.full']:
            candidate = example_dir / name
            if candidate.exists():
                system_cfg = candidate
                break
    
    if system_cfg.exists():
        cfg_info = parse_system_cfg(system_cfg)
        info['config'] = cfg_info
    
    return info

def analyze_examples(examples_dir: Path) -> List[Dict]:
    """Analyze all examples in the examples directory."""
    examples = []
    
    if not examples_dir.exists():
        print(f"Error: Examples directory not found: {examples_dir}", file=sys.stderr)
        return examples
    
    for item in examples_dir.iterdir():
        if item.is_dir():
            info = analyze_example_dir(item)
            if info['pdb_files'] or info['config_files']:
                examples.append(info)
    
    examples.sort(key=lambda x: x['name'])
    return examples

def print_summary(examples: List[Dict], examples_dir: Path):
    """Print summary statistics."""
    print("=== OSPREY Example Workload Analysis ===\n")
    print(f"Total examples analyzed: {len(examples)}\n")
    
    # Group by type
    by_type = defaultdict(list)
    for ex in examples:
        by_type[ex['type']].append(ex)
    
    print("=== Summary by Type ===")
    for type_name in sorted(by_type.keys()):
        count = len(by_type[type_name])
        print(f"{type_name:15s}: {count:3d} examples")
    print()
    
    # Atom count statistics
    all_atom_counts = []
    for ex in examples:
        all_atom_counts.extend(ex['atom_counts'])
    
    if all_atom_counts:
        print("=== Atom Count Statistics (from PDB files) ===")
        print(f"Total PDB files: {len(all_atom_counts)}")
        print(f"Min atoms:       {min(all_atom_counts):,}")
        print(f"Max atoms:       {max(all_atom_counts):,}")
        print(f"Mean atoms:      {sum(all_atom_counts) / len(all_atom_counts):,.0f}")
        print(f"Median atoms:    {sorted(all_atom_counts)[len(all_atom_counts) // 2]:,}")
        print()

        # Percentiles (use numpy for stable quantiles)
        try:
            counts_sorted = sorted(all_atom_counts)
            percentiles = [0, 10, 25, 50, 75, 90, 95, 99, 100]
            print("=== Atom Count Percentiles ===")
            for p in percentiles:
                q = int(np.quantile(counts_sorted, p / 100.0))
                print(f"p{p:02d}: {q:,}")
            print()
        except Exception:
            pass
        
        # Distribution buckets
        buckets = [
            (0, 100, "Tiny (<100)"),
            (100, 500, "Small (100-500)"),
            (500, 1000, "Medium (500-1K)"),
            (1000, 5000, "Large (1K-5K)"),
            (5000, 20000, "Very Large (5K-20K)"),
            (20000, float('inf'), "Huge (20K+)")
        ]
        
        print("=== Atom Count Distribution ===")
        for min_val, max_val, label in buckets:
            count = sum(1 for c in all_atom_counts if min_val <= c < max_val)
            pct = (count / len(all_atom_counts)) * 100 if all_atom_counts else 0
            print(f"{label:20s}: {count:3d} files ({pct:5.1f}%)")
        print()
    
    # Detailed list
    print("=== Detailed Example List ===")
    print(f"{'Example':<30} {'Type':<12} {'PDBs':<6} {'Atoms (min)':<12} {'Atoms (max)':<12} {'Configs':<6}")
    print("-" * 100)
    
    for ex in examples:
        min_atoms = min(ex['atom_counts']) if ex['atom_counts'] else 0
        max_atoms = max(ex['atom_counts']) if ex['atom_counts'] else 0
        print(f"{ex['name']:<30} {ex['type']:<12} {len(ex['pdb_files']):<6} "
              f"{min_atoms:<12,} {max_atoms:<12,} {len(ex['config_files']):<6}")
    
    print()
    print("=== Notes ===")
    print("- Atom counts are from PDB files (ATOM/HETATM records)")
    print("- Actual workload sizes (atom pairs) depend on conformation space setup")
    print("- For actual pair counts, need to execute examples or inspect ConfSpace")
    print("- Test cases: 2RL0 and 1DG9_6f are used in TestNativeConfEnergyCalculator")
    
    # Generate plot
    if all_atom_counts:
        plot_distribution(all_atom_counts, examples_dir)

def plot_distribution(atom_counts: List[int], output_dir: Path):
    """Create visualization of atom count distribution."""
    try:
        # Create output directory for plots
        plots_dir = output_dir / 'plots'
        plots_dir.mkdir(exist_ok=True)
        
        fig, axes = plt.subplots(2, 1, figsize=(10, 10))
        
        # Histogram
        ax1 = axes[0]
        bins = np.logspace(np.log10(min(atom_counts)), np.log10(max(atom_counts)), 30)
        ax1.hist(atom_counts, bins=bins, edgecolor='black', alpha=0.7, color='steelblue')
        ax1.set_xscale('log')
        ax1.set_xlabel('Number of Atoms (log scale)', fontsize=12)
        ax1.set_ylabel('Number of PDB Files', fontsize=12)
        ax1.set_title('Distribution of System Sizes (Atom Counts)', fontsize=14, fontweight='bold')
        ax1.grid(True, alpha=0.3, linestyle='--')
        
        # Add statistics text
        stats_text = f'Total: {len(atom_counts)} files\n'
        stats_text += f'Min: {min(atom_counts):,} atoms\n'
        stats_text += f'Max: {max(atom_counts):,} atoms\n'
        stats_text += f'Mean: {sum(atom_counts) / len(atom_counts):,.0f} atoms\n'
        stats_text += f'Median: {sorted(atom_counts)[len(atom_counts) // 2]:,} atoms'
        ax1.text(0.98, 0.98, stats_text, transform=ax1.transAxes,
                verticalalignment='top', horizontalalignment='right',
                bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
                fontsize=10, family='monospace')
        
        # Box plot
        ax2 = axes[1]
        bp = ax2.boxplot(atom_counts, vert=True, patch_artist=True,
                        boxprops=dict(facecolor='lightblue', alpha=0.7),
                        medianprops=dict(color='red', linewidth=2),
                        whiskerprops=dict(color='black', linewidth=1.5),
                        capprops=dict(color='black', linewidth=1.5))
        ax2.set_yscale('log')
        ax2.set_ylabel('Number of Atoms (log scale)', fontsize=12)
        ax2.set_title('System Size Distribution (Box Plot)', fontsize=14, fontweight='bold')
        ax2.grid(True, alpha=0.3, linestyle='--', axis='y')
        ax2.set_xticklabels(['All Examples'])
        
        plt.tight_layout()
        
        # Save plot
        plot_path = plots_dir / 'example_workload_distribution.png'
        plt.savefig(plot_path, dpi=150, bbox_inches='tight')
        print(f"\nPlot saved to: {plot_path}")
        
        # Also create a linear scale version for better visibility of smaller systems
        fig2, ax = plt.subplots(1, 1, figsize=(10, 6))
        # Use linear bins for smaller range
        bins_linear = np.linspace(0, max(atom_counts), 50)
        ax.hist(atom_counts, bins=bins_linear, edgecolor='black', alpha=0.7, color='steelblue')
        ax.set_xlabel('Number of Atoms', fontsize=12)
        ax.set_ylabel('Number of PDB Files', fontsize=12)
        ax.set_title('Distribution of System Sizes (Linear Scale)', fontsize=14, fontweight='bold')
        ax.grid(True, alpha=0.3, linestyle='--')
        
        # Add vertical lines for statistics
        mean_val = sum(atom_counts) / len(atom_counts)
        median_val = sorted(atom_counts)[len(atom_counts) // 2]
        ax.axvline(mean_val, color='red', linestyle='--', linewidth=2, label=f'Mean: {mean_val:,.0f}')
        ax.axvline(median_val, color='green', linestyle='--', linewidth=2, label=f'Median: {median_val:,}')
        ax.legend(fontsize=10)
        
        plt.tight_layout()
        plot_path_linear = plots_dir / 'example_workload_distribution_linear.png'
        plt.savefig(plot_path_linear, dpi=150, bbox_inches='tight')
        print(f"Linear scale plot saved to: {plot_path_linear}")
        
        plt.close('all')
        
    except ImportError:
        print("\nWarning: matplotlib not available. Skipping plot generation.")
        print("Install with: pip install matplotlib numpy")
    except Exception as e:
        print(f"\nWarning: Could not generate plot: {e}")

def main():
    if len(sys.argv) > 1:
        examples_dir = Path(sys.argv[1])
    else:
        # Default: look for examples/ relative to script location
        script_dir = Path(__file__).parent.parent.parent
        examples_dir = script_dir / 'examples'
    
    examples = analyze_examples(examples_dir)
    print_summary(examples, examples_dir)

if __name__ == '__main__':
    main()

