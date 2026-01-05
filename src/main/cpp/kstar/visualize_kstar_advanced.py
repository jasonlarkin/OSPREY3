#!/usr/bin/env python3
"""
Advanced visualization script for K* computation results.

Provides additional visualizations including convergence plots and energy matrix heatmaps.
Requires: matplotlib, numpy, seaborn (optional, for heatmaps)
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
    import matplotlib.colors as mcolors
except ImportError as e:
    print(f"ERROR: Missing visualization dependencies: {e}")
    print(f"Install with: pip install matplotlib numpy")
    sys.exit(1)

try:
    import seaborn as sns
    HAS_SEABORN = True
except ImportError:
    HAS_SEABORN = False
    print("WARNING: seaborn not available, heatmaps will use matplotlib only")


def plot_energy_matrix_heatmap(
    emat,
    title="Energy Matrix",
    max_positions=20,
    max_confs_per_pos=40,
    clip_percentiles=(1.0, 99.0),
):
    """
    Plot energy matrix as heatmap (for small matrices).
    
    Args:
        emat: EnergyMatrix object
        title: Plot title
        max_positions: Maximum number of positions to visualize (for performance)
    """
    num_pos = emat.get_num_positions()
    
    if num_pos > max_positions:
        print(f"WARNING: Matrix has {num_pos} positions, limiting visualization to {max_positions}")
        num_pos = max_positions
    
    # Extract conformations per position
    num_confs_per_pos = [emat.get_num_confs_at_pos(pos) for pos in range(num_pos)]
    total_confs = sum(num_confs_per_pos)
    
    if total_confs > 1000:
        print(f"WARNING: Total conformations ({total_confs}) is large, heatmap may be slow")
        print("Consider using plot_energy_matrix_summary() instead")
    
    fig, axes = plt.subplots(1, 2, figsize=(14, 6))
    fig.suptitle(f'{title} Energy Matrix Visualization', fontsize=14, fontweight='bold')
    
    # Panel 1: Conformations per position bar chart
    ax = axes[0]
    positions = list(range(num_pos))
    bars = ax.bar(positions, num_confs_per_pos, alpha=0.7, color='steelblue')
    ax.set_xlabel('Position')
    ax.set_ylabel('Number of Conformations')
    ax.set_title('Conformations per Position')
    ax.set_xticks(positions)
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add labels on bars
    for bar, num in zip(bars, num_confs_per_pos):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{num}',
                ha='center', va='bottom', fontsize=9)
    
    # Panel 2: One-body energies heatmap (pos x conf)
    ax = axes[1]

    max_confs = min(max(num_confs_per_pos), max_confs_per_pos)
    # Pad ragged conf counts with NaN so we can render as an image.
    data = np.full((num_pos, max_confs), np.nan, dtype=float)
    for pos in range(num_pos):
        n = min(num_confs_per_pos[pos], max_confs)
        for conf in range(n):
            data[pos, conf] = emat.get_one_body(pos, conf)

    masked = np.ma.masked_invalid(data)

    # Robust color scaling: clamp to percentiles so single outliers don't dominate.
    finite_vals = data[np.isfinite(data)]
    vmin = None
    vmax = None
    outlier_note = ""
    if finite_vals.size > 0 and clip_percentiles is not None:
        lo_p, hi_p = clip_percentiles
        lo = float(np.percentile(finite_vals, lo_p))
        hi = float(np.percentile(finite_vals, hi_p))
        # Avoid degenerate ranges.
        if hi <= lo:
            lo = float(np.min(finite_vals))
            hi = float(np.max(finite_vals))
        vmin, vmax = lo, hi
        n_lo = int(np.sum(finite_vals < lo))
        n_hi = int(np.sum(finite_vals > hi))
        if n_lo + n_hi > 0:
            outlier_note = f" (clipped: {n_lo} low, {n_hi} high)"

    if HAS_SEABORN:
        sns.heatmap(
            masked,
            ax=ax,
            cmap="viridis",
            cbar=True,
            cbar_kws={"label": "One-body energy"},
            xticklabels=False,
            yticklabels=[str(p) for p in range(num_pos)],
            vmin=vmin,
            vmax=vmax,
        )
    else:
        norm = mcolors.Normalize(vmin=vmin, vmax=vmax) if (vmin is not None and vmax is not None) else None
        im = ax.imshow(masked, aspect="auto", interpolation="nearest", cmap="viridis", norm=norm)
        cbar = fig.colorbar(im, ax=ax)
        cbar.set_label("One-body energy")
        ax.set_yticks(range(num_pos))
        ax.set_yticklabels([str(p) for p in range(num_pos)])

    ax.set_xlabel("Conformation index (truncated)")
    ax.set_ylabel("Position")
    ax.set_title(f"One-Body Energies (max {max_confs} confs/pos){outlier_note}")
    
    plt.tight_layout()
    return fig


def plot_convergence_comparison(result1, result2, label1="Run 1", label2="Run 2"):
    """
    Compare two K* workflow results (for benchmarking different methods/options).
    
    Args:
        result1: First KStarWorkflowResult
        result2: Second KStarWorkflowResult
        label1: Label for first result
        label2: Label for second result
    """
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle('K* Results Comparison', fontsize=14, fontweight='bold')
    
    components = ['protein', 'ligand', 'complex']
    
    # Panel 1: Partition function bounds comparison
    ax = axes[0, 0]
    x = np.arange(len(components))
    width = 0.35
    
    for i, comp in enumerate(components):
        r1_pfunc = getattr(result1.pfuncs, comp)
        r2_pfunc = getattr(result2.pfuncs, comp)
        
        r1_lower = r1_pfunc.lower_bound
        r2_lower = r2_pfunc.lower_bound
        
        ax.bar(x[i] - width/2, r1_lower, width, label=label1 if i == 0 else '', alpha=0.7)
        ax.bar(x[i] + width/2, r2_lower, width, label=label2 if i == 0 else '', alpha=0.7)
    
    ax.set_xlabel('Component')
    ax.set_ylabel('log10(Q) lower bound')
    ax.set_title('Partition Function Bounds Comparison')
    ax.set_xticks(x)
    ax.set_xticklabels(components)
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    # Panel 2: Convergence delta comparison
    ax = axes[0, 1]
    deltas1 = [getattr(result1.pfuncs, c).delta for c in components]
    deltas2 = [getattr(result2.pfuncs, c).delta for c in components]
    
    x = np.arange(len(components))
    ax.bar(x - width/2, deltas1, width, label=label1, alpha=0.7)
    ax.bar(x + width/2, deltas2, width, label=label2, alpha=0.7)
    ax.set_yscale('log')
    ax.set_xlabel('Component')
    ax.set_ylabel('Delta (convergence gap)')
    ax.set_title('Convergence Delta Comparison')
    ax.set_xticks(x)
    ax.set_xticklabels(components)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    
    # Panel 3: Conformations explored comparison
    ax = axes[1, 0]
    confs1 = [getattr(result1.pfuncs, c).num_confs for c in components]
    confs2 = [getattr(result2.pfuncs, c).num_confs for c in components]
    
    x = np.arange(len(components))
    ax.bar(x - width/2, confs1, width, label=label1, alpha=0.7)
    ax.bar(x + width/2, confs2, width, label=label2, alpha=0.7)
    ax.set_yscale('log')
    ax.set_xlabel('Component')
    ax.set_ylabel('Number of Conformations')
    ax.set_title('Conformations Explored Comparison')
    ax.set_xticks(x)
    ax.set_xticklabels(components)
    ax.legend()
    ax.grid(True, alpha=0.3, axis='y')
    
    # Panel 4: K* score comparison
    ax = axes[1, 1]
    kstar_values = [result1.log10_value, result2.log10_value]
    kstar_labels = [label1, label2]
    
    bars = ax.bar(kstar_labels, kstar_values, alpha=0.7, color=['steelblue', 'coral'])
    ax.set_ylabel('log10(K*)')
    ax.set_title('K* Score Comparison')
    ax.grid(True, alpha=0.3, axis='y')
    
    # Add value labels
    for bar, val in zip(bars, kstar_values):
        height = bar.get_height()
        ax.text(bar.get_x() + bar.get_width()/2., height,
                f'{val:.4f}',
                ha='center', va='bottom', fontsize=10)
    
    # Add difference annotation
    diff = abs(kstar_values[0] - kstar_values[1])
    ax.text(0.5, 0.02, f'Absolute difference: {diff:.6f}',
            ha='center', transform=ax.transAxes,
            bbox=dict(boxstyle='round', facecolor='yellow', alpha=0.3),
            fontsize=9)
    
    plt.tight_layout()
    return fig


def plot_epsilon_sweep(protein_file, ligand_file, complex_file, epsilons):
    """
    Plot computation time and convergence behavior across different epsilon values.
    
    Args:
        protein_file: Path to protein.emat.bin
        ligand_file: Path to ligand.emat.bin
        complex_file: Path to complex.emat.bin
        epsilons: List of epsilon values to test
    """
    print("Loading energy matrices...")
    protein = kstar_cpp.load_energy_matrix(protein_file)
    ligand = kstar_cpp.load_energy_matrix(ligand_file)
    complex_emat = kstar_cpp.load_energy_matrix(complex_file)
    
    workflow = kstar_cpp.KStarWorkflow()
    
    times = []
    kstar_values = []
    total_confs = []
    all_converged = []
    
    print("Computing K* for different epsilon values...")
    for epsilon in epsilons:
        start = time.perf_counter()
        result = workflow.compute(protein, ligand, complex_emat, epsilon=epsilon)
        elapsed = time.perf_counter() - start
        
        times.append(elapsed)
        kstar_values.append(result.log10_value)
        total_confs.append(result.pfuncs.protein.num_confs + 
                          result.pfuncs.ligand.num_confs + 
                          result.pfuncs.complex.num_confs)
        all_converged.append(result.converged)
        
        print(f"  epsilon={epsilon:.3f}: {elapsed:.3f}s, converged={result.converged}")
    
    fig, axes = plt.subplots(2, 2, figsize=(12, 10))
    fig.suptitle('Epsilon Parameter Sweep', fontsize=14, fontweight='bold')
    
    # Panel 1: Computation time vs epsilon
    ax = axes[0, 0]
    ax.plot(epsilons, times, 'o-', linewidth=2, markersize=8)
    ax.set_xlabel('Epsilon')
    ax.set_ylabel('Computation Time (s)')
    ax.set_title('Time vs Epsilon')
    ax.grid(True, alpha=0.3)
    ax.set_xscale('log')
    
    # Panel 2: Total conformations explored vs epsilon
    ax = axes[0, 1]
    ax.plot(epsilons, total_confs, 'o-', linewidth=2, markersize=8, color='coral')
    ax.set_xlabel('Epsilon')
    ax.set_ylabel('Total Conformations Explored')
    ax.set_title('Conformations vs Epsilon')
    ax.set_yscale('log')
    ax.set_xscale('log')
    ax.grid(True, alpha=0.3)
    
    # Panel 3: K* value vs epsilon
    ax = axes[1, 0]
    ax.plot(epsilons, kstar_values, 'o-', linewidth=2, markersize=8, color='green')
    ax.set_xlabel('Epsilon')
    ax.set_ylabel('log10(K*)')
    ax.set_title('K* Value vs Epsilon')
    ax.grid(True, alpha=0.3)
    ax.set_xscale('log')
    
    # Panel 4: Convergence status
    ax = axes[1, 1]
    converged_binary = [1 if c else 0 for c in all_converged]
    ax.bar(range(len(epsilons)), converged_binary, alpha=0.7, color=['red' if not c else 'green' for c in all_converged])
    ax.set_xlabel('Epsilon Index')
    ax.set_ylabel('Converged (1=Yes, 0=No)')
    ax.set_title('Convergence Status')
    ax.set_xticks(range(len(epsilons)))
    ax.set_xticklabels([f'{e:.3f}' for e in epsilons], rotation=45, ha='right')
    ax.set_ylim(-0.1, 1.1)
    ax.grid(True, alpha=0.3, axis='y')
    
    plt.tight_layout()
    return fig


if __name__ == '__main__':
    import argparse
    import time
    
    parser = argparse.ArgumentParser(
        description='Advanced visualization for K* computation results'
    )
    parser.add_argument('protein_emat', help='Path to protein.emat.bin')
    parser.add_argument('ligand_emat', help='Path to ligand.emat.bin')
    parser.add_argument('complex_emat', help='Path to complex.emat.bin')
    parser.add_argument('--epsilon', type=float, default=0.99,
                       help='Epsilon for partition function (default: 0.99)')
    parser.add_argument('--output', '-o', help='Output file prefix (default: display)')
    parser.add_argument('--dpi', type=int, default=75,
                       help='DPI for output images (default: 75)')
    parser.add_argument('--heatmap', action='store_true',
                       help='Generate one-body energy heatmap (pos x conf, truncated)')
    parser.add_argument('--heatmap-max-confs', type=int, default=40,
                       help='Max conformations per position to plot in heatmap (default: 40)')
    parser.add_argument('--heatmap-clip', type=str, default="1,99",
                       help='Percentile clamp for heatmap color scale as "lo,hi" (default: "1,99"). '
                            'Use "0,100" to disable clipping.')
    parser.add_argument('--compare-variants', action='store_true',
                       help='Compare Baseline vs Fast A* variants')
    parser.add_argument('--epsilon-sweep', action='store_true',
                       help='Plot epsilon parameter sweep')
    parser.add_argument('--epsilon-sweep-values', type=str, default="",
                       help='Comma-separated epsilon values for --epsilon-sweep (e.g. "0.001,0.005,0.01,0.02,0.05"). '
                            'If omitted, defaults to "0.90,0.95,0.99,0.999".')
    
    args = parser.parse_args()
    
    print("Loading energy matrices...")
    protein_emat = kstar_cpp.load_energy_matrix(args.protein_emat)
    ligand_emat = kstar_cpp.load_energy_matrix(args.ligand_emat)
    complex_emat = kstar_cpp.load_energy_matrix(args.complex_emat)
    
    workflow = kstar_cpp.KStarWorkflow()
    
    # Compute baseline result
    print(f"Computing K* score (epsilon={args.epsilon})...")
    result_baseline = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=args.epsilon)
    
    print(f"\nK* Results:")
    print(f"  log10(K*) = {result_baseline.log10_value:.6f}")
    print(f"  Bounds: [{result_baseline.log10_lower_bound:.6f}, {result_baseline.log10_upper_bound:.6f}]")
    print(f"  Converged: {result_baseline.converged}")
    
    figures = []
    
    # Energy matrix heatmap (if requested)
    if args.heatmap:
        print("\nCreating energy matrix heatmap...")
        clip = None
        try:
            lo_s, hi_s = [x.strip() for x in args.heatmap_clip.split(",")]
            lo_p = float(lo_s)
            hi_p = float(hi_s)
            if lo_p <= 0.0 and hi_p >= 100.0:
                clip = None
            else:
                clip = (lo_p, hi_p)
        except Exception:
            clip = (1.0, 99.0)

        fig = plot_energy_matrix_heatmap(
            protein_emat,
            "Protein",
            max_confs_per_pos=args.heatmap_max_confs,
            clip_percentiles=clip,
        )
        figures.append(("heatmap", fig))
    
    # Compare variants
    if args.compare_variants:
        print("\nComparing A* variants...")
        options_baseline = kstar_cpp.PartitionFunctionOptions()
        options_baseline.astar_variant = kstar_cpp.AStarVariant.Baseline
        options_baseline.allow_exact_enumeration = False
        
        options_fast = kstar_cpp.PartitionFunctionOptions()
        options_fast.astar_variant = kstar_cpp.AStarVariant.Fast
        options_fast.allow_exact_enumeration = False
        
        result_fast = workflow.compute(protein_emat, ligand_emat, complex_emat, epsilon=args.epsilon,
                                       method=kstar_cpp.PartitionFunctionMethod.AStar,
                                       options=options_fast)
        
        fig = plot_convergence_comparison(result_baseline, result_fast, "Baseline", "Fast")
        figures.append(("variant_comparison", fig))
    
    # Epsilon sweep
    if args.epsilon_sweep:
        print("\nRunning epsilon sweep...")
        if args.epsilon_sweep_values.strip():
            epsilons = [float(x) for x in args.epsilon_sweep_values.split(",") if x.strip()]
        else:
            epsilons = [0.90, 0.95, 0.99, 0.999]
        fig = plot_epsilon_sweep(args.protein_emat, args.ligand_emat, args.complex_emat, epsilons)
        figures.append(("epsilon_sweep", fig))
    
    # Save or display
    if args.output:
        for name, fig in figures:
            fig.savefig(f"{args.output}_{name}.png", dpi=args.dpi,
                       facecolor='white', edgecolor='none')
            plt.close(fig)
        print(f"\nPlots saved to {args.output}_*.png (DPI: {args.dpi})")
    else:
        plt.show()
