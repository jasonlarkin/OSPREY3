#!/usr/bin/env python3
"""
Visualization script for K* computation results.

This script creates plots from K* workflow results.
Requires: matplotlib, numpy
"""

import sys
import os

# Add build directory to path
build_dir = os.path.join(os.path.dirname(__file__), "../../../../build/cpp/kstar-python")
if os.path.exists(build_dir):
    sys.path.insert(0, build_dir)

try:
    import kstar_cpp
except ImportError as e:
    print(f"ERROR: Failed to import kstar_cpp: {e}")
    print(f"Make sure the module is built and in your PYTHONPATH")
    sys.exit(1)

try:
    import matplotlib.pyplot as plt
    import numpy as np
except ImportError as e:
    print(f"ERROR: Missing visualization dependencies: {e}")
    print(f"Install with: pip install matplotlib numpy")
    sys.exit(1)


def plot_kstar_results(protein_result, ligand_result, complex_result, kstar_result):
    """
    Create a multi-panel plot showing K* computation results.
    
    Args:
        protein_result: PartitionFunctionResult for protein
        ligand_result: PartitionFunctionResult for ligand
        complex_result: PartitionFunctionResult for complex
        kstar_result: KStarWorkflowResult
    """
    fig, axes = plt.subplots(2, 2, figsize=(11, 8.5), constrained_layout=True)
    fig.suptitle('K* Computation Results', fontsize=14, fontweight='bold')
    
    # Panel 1: Partition function bounds (bar chart)
    ax = axes[0, 0]
    pfuncs = [
        ('Protein', protein_result.lower_bound, protein_result.upper_bound),
        ('Ligand', ligand_result.lower_bound, ligand_result.upper_bound),
        ('Complex', complex_result.lower_bound, complex_result.upper_bound),
    ]
    
    names = [p[0] for p in pfuncs]
    lower_bounds = [p[1] for p in pfuncs]
    upper_bounds = [p[2] for p in pfuncs]
    
    x_pos = np.arange(len(names))
    width = 0.35
    
    ax.bar(x_pos - width/2, lower_bounds, width, label='Lower bound', alpha=0.8)
    ax.bar(x_pos + width/2, upper_bounds, width, label='Upper bound', alpha=0.8)
    ax.set_xlabel('Component')
    ax.set_ylabel('log10(Q)')
    ax.set_title('Partition Function Bounds')
    ax.set_xticks(x_pos)
    ax.set_xticklabels(names)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Panel 2: Convergence delta
    ax = axes[0, 1]
    deltas = [protein_result.delta, ligand_result.delta, complex_result.delta]
    converged = [protein_result.converged, ligand_result.converged, complex_result.converged]
    
    colors = ['green' if c else 'red' for c in converged]
    bars = ax.bar(names, deltas, color=colors, alpha=0.7)
    ax.axhline(y=0.01, color='orange', linestyle='--', label='1% threshold')
    ax.set_yscale('log')
    ax.set_xlabel('Component')
    ax.set_ylabel('Delta (convergence gap)')
    ax.set_title('Convergence Delta (lower is better)')
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add labels on bars
    for i, (bar, delta, conv) in enumerate(zip(bars, deltas, converged)):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{delta:.4f}\n{"CONV" if conv else "NC"}',
                ha='center', va='bottom', fontsize=9)
    
    # Panel 3: Conformations explored
    ax = axes[1, 0]
    num_confs = [
        protein_result.num_confs,
        ligand_result.num_confs,
        complex_result.num_confs,
    ]
    
    bars = ax.bar(names, num_confs, alpha=0.7, color='steelblue')
    ax.set_xlabel('Component')
    ax.set_ylabel('Number of Conformations')
    ax.set_title('Conformations Explored')
    ax.set_yscale('log')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add labels on bars
    for bar, num in zip(bars, num_confs):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{num:,}',
                ha='center', va='bottom', fontsize=9, rotation=90)
    
    # Panel 4: K* score bounds
    ax = axes[1, 1]
    kstar_lower = kstar_result.log10_lower_bound
    kstar_upper = kstar_result.log10_upper_bound
    kstar_value = kstar_result.log10_value
    
    # Create error bar style visualization
    ax.errorbar(0, kstar_value, 
                yerr=[[kstar_value - kstar_lower], [kstar_upper - kstar_value]],
                fmt='o', markersize=10, capsize=10, capthick=2,
                label=f'K* = 10^{kstar_value:.4f}')
    
    # Fill region between bounds
    ax.fill_between([-0.5, 0.5], kstar_lower, kstar_upper, 
                     alpha=0.2, label='Bounds')
    
    ax.axhline(y=kstar_value, color='blue', linestyle='--', alpha=0.5, label='Point estimate')
    ax.set_xlim(-0.5, 0.5)
    ax.set_ylabel('log10(K*)')
    ax.set_title(f'K* Score (converged: {kstar_result.converged})')
    ax.legend(loc='best')
    ax.grid(True, alpha=0.3)
    ax.set_xticks([])
    
    # Add text annotation
    ax.text(0, kstar_value + 0.5 * (kstar_upper - kstar_lower),
            f'K* = 10^{kstar_value:.4f}\n'
            f'Bounds: [10^{kstar_lower:.4f}, 10^{kstar_upper:.4f}]\n'
            f'Converged: {kstar_result.converged}',
            ha='center', va='center', 
            bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5),
            fontsize=10)
    
    plt.tight_layout(rect=[0, 0.03, 1, 0.97])
    return fig


def plot_energy_matrix_summary(emat, title="Energy Matrix"):
    """
    Plot summary statistics for an energy matrix.
    
    Args:
        emat: EnergyMatrix object
        title: Plot title
    """
    num_pos = emat.get_num_positions()
    positions = list(range(num_pos))
    num_confs_per_pos = [emat.get_num_confs_at_pos(pos) for pos in positions]
    
    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(10, 4), constrained_layout=True)
    fig.suptitle(f'{title} Summary', fontsize=14, fontweight='bold')
    
    # Panel 1: Positions vs conformations
    ax1.bar(positions, num_confs_per_pos, alpha=0.7, color='steelblue')
    ax1.set_xlabel('Position')
    ax1.set_ylabel('Number of Conformations')
    ax1.set_title('Conformations per Position')
    ax1.grid(True, alpha=0.3, axis='y')
    ax1.set_xticks(positions)
    
    # Panel 2: Statistics
    ax2.axis('off')
    
    stats_text = (
        f"Statistics:\n\n"
        f"Total Positions: {num_pos}\n"
        f"Total Conformations: {sum(num_confs_per_pos)}\n"
        f"Max Confs/Position: {max(num_confs_per_pos)}\n"
        f"Min Confs/Position: {min(num_confs_per_pos)}\n"
        f"Avg Confs/Position: {sum(num_confs_per_pos) / num_pos:.1f}\n"
        f"Constant Term: {emat.get_const_term():.4f}"
    )
    
    ax2.text(0.1, 0.5, stats_text, fontsize=11, family='monospace',
            verticalalignment='center', transform=ax2.transAxes)
    
    return fig


if __name__ == '__main__':
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Visualize K* computation results from .emat.bin files'
    )
    parser.add_argument('protein_emat', help='Path to protein.emat.bin')
    parser.add_argument('ligand_emat', help='Path to ligand.emat.bin')
    parser.add_argument('complex_emat', help='Path to complex.emat.bin')
    parser.add_argument('--epsilon', type=float, default=0.99,
                       help='Epsilon for partition function (default: 0.99)')
    parser.add_argument('--output', '-o', help='Output file (default: display)')
    parser.add_argument('--dpi', type=int, default=75,
                       help='DPI for output images (default: 75, use 100-150 for better quality)')
    
    args = parser.parse_args()
    
    print(f"Loading energy matrices...")
    protein_emat = kstar_cpp.load_energy_matrix(args.protein_emat)
    ligand_emat = kstar_cpp.load_energy_matrix(args.ligand_emat)
    complex_emat = kstar_cpp.load_energy_matrix(args.complex_emat)
    
    print(f"Computing K* score...")
    workflow = kstar_cpp.KStarWorkflow()
    result = workflow.compute(
        protein=protein_emat,
        ligand=ligand_emat,
        complex=complex_emat,
        epsilon=args.epsilon
    )
    
    print(f"\nK* Results:")
    print(f"  log10(K*) = {result.log10_value:.6f}")
    print(f"  Bounds: [{result.log10_lower_bound:.6f}, {result.log10_upper_bound:.6f}]")
    print(f"  Converged: {result.converged}")
    
    print(f"\nCreating plots...")
    fig1 = plot_kstar_results(
        result.pfuncs.protein,
        result.pfuncs.ligand,
        result.pfuncs.complex,
        result
    )
    
    fig2 = plot_energy_matrix_summary(protein_emat, "Protein")
    
    if args.output:
        fig1.savefig(f"{args.output}_kstar.png", dpi=args.dpi, 
                    facecolor='white', edgecolor='none')
        plt.close(fig1)
        fig2.savefig(f"{args.output}_protein_emat.png", dpi=args.dpi,
                    facecolor='white', edgecolor='none')
        plt.close(fig2)
        print(f"\nPlots saved to {args.output}_*.png (DPI: {args.dpi})")
    else:
        plt.show()
