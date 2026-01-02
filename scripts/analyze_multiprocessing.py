#!/usr/bin/env python3
"""
Analyze multiprocessing opportunities in OSPREY pipeline.

Identifies:
- Parallelizable stages (matches, sequences)
- MPI rank assignment strategies
- Threading opportunities within stages
- Shared memory requirements
- Resource lifetime + allocation-churn considerations per rank/task (approach TBD)
"""

import json
import argparse
from pathlib import Path
from typing import Dict, List, Optional
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from collections import defaultdict

def analyze_sequence_parallelism(kstar_config: Dict) -> Dict:
    """Analyze sequence-level parallelism opportunities."""
    analysis = {
        "total_sequences": kstar_config.get("num_sequences", 0),
        "independent_sequences": True,  # Sequences are independent
        "shared_data": {
            "energy_matrices": True,
            "confspace": True
        },
        "parallelism_strategy": "embarrassingly_parallel",
        "expected_speedup": min(8, kstar_config.get("num_sequences", 1))
    }
    
    return analysis

def analyze_match_parallelism(montage_config: Dict) -> Dict:
    """Analyze match-level parallelism opportunities."""
    analysis = {
        "total_matches": montage_config.get("num_matches", 0),
        "independent_matches": True,
        "current_bottleneck": "sequential_processing",
        "jvm_overhead_per_match": 1.5,  # seconds
        "parallelism_strategy": "mpi_multi_process",
        "expected_speedup": min(10, montage_config.get("num_matches", 1))
    }
    
    return analysis

def design_mpi_architecture(config: Dict) -> Dict:
    """Design MPI architecture for unified pipeline."""
    architecture = {
        "ranks": config.get("num_ranks", 4),
        "rank_assignments": [],
        "shared_memory_windows": [],
        "resource_lifetimes": {}
    }
    
    num_ranks = architecture["ranks"]
    num_matches = config.get("num_matches", 10)
    
    # Assign matches to ranks
    matches_per_rank = (num_matches + num_ranks - 1) // num_ranks
    
    for rank in range(num_ranks):
        start_match = rank * matches_per_rank
        end_match = min(start_match + matches_per_rank, num_matches)
        
        rank_config = {
            "rank": rank,
            "is_master": rank == 0,
            "matches": list(range(start_match, end_match)),
            "threads_per_rank": config.get("threads_per_rank", 4),
            "scratch": {
                "type": "per_rank",
                "size_mb": 2048,  # 2GB per rank (placeholder sizing)
                "lifetime": "entire_run"
            }
        }
        
        architecture["rank_assignments"].append(rank_config)
    
    # Shared memory windows
    architecture["shared_memory_windows"] = [
        {
            "name": "energy_matrices",
            "size_mb": 5000,
            "access": "read_only",
            "ranks": "all"
        },
        {
            "name": "confspace",
            "size_mb": 1000,
            "access": "read_only",
            "ranks": "all"
        },
        {
            "name": "results",
            "size_mb": 100,
            "access": "write",
            "ranks": "all"
        }
    ]
    
    # Per-sequence scratch resources (within each rank)
    architecture["resource_lifetimes"] = {
        "per_rank_resources": {
            "energy_matrices": {"size_mb": 2000, "lifetime": "entire_run"},
            "confspace": {"size_mb": 500, "lifetime": "entire_run"}
        },
        "per_sequence_resources": {
            "astar_tree": {"size_mb": 200, "lifetime": "per_sequence"},
            "partition_function": {"size_mb": 50, "lifetime": "per_sequence"}
        },
        "per_task_arenas": {
            "scope_hulls": {"size_mb": 10, "lifetime": "per_task"},
            "montage_scaffolds": {"size_mb": 50, "lifetime": "per_task"}
        }
    }
    
    return architecture

def plot_mpi_architecture(architecture: Dict, output_file: Path):
    """Plot MPI architecture diagram."""
    fig, ax = plt.subplots(figsize=(14, 10))
    
    num_ranks = len(architecture["rank_assignments"])
    
    # Draw ranks
    rank_width = 0.8
    rank_height = 1.5
    spacing = 0.2
    
    for i, rank_config in enumerate(architecture["rank_assignments"]):
        x = i * (rank_width + spacing)
        y = 0
        
        # Rank box
        rank_color = 'lightblue' if rank_config["is_master"] else 'lightgreen'
        rect = plt.Rectangle((x, y), rank_width, rank_height,
                           facecolor=rank_color, edgecolor='black', linewidth=2)
        ax.add_patch(rect)
        
        # Rank label
        rank_label = f"Rank {rank_config['rank']}"
        if rank_config["is_master"]:
            rank_label += " (Master)"
        ax.text(x + rank_width/2, y + rank_height - 0.2, rank_label,
               ha='center', va='top', fontsize=10, weight='bold')
        
        # Matches
        matches_str = f"Matches: {rank_config['matches']}"
        ax.text(x + rank_width/2, y + rank_height - 0.5, matches_str,
               ha='center', va='top', fontsize=8)
        
        # Threads
        threads_str = f"Threads: {rank_config['threads_per_rank']}"
        ax.text(x + rank_width/2, y + rank_height - 0.7, threads_str,
               ha='center', va='top', fontsize=8)
        
        # Scratch sizing info (placeholder)
        arena_str = f"Scratch: {rank_config['scratch']['size_mb']}MB"
        ax.text(x + rank_width/2, y + 0.1, arena_str,
               ha='center', va='bottom', fontsize=7, style='italic')
    
    # Shared memory windows (above ranks)
    shared_y = rank_height + 0.5
    shared_width = num_ranks * (rank_width + spacing) - spacing
    
    for i, window in enumerate(architecture["shared_memory_windows"]):
        window_y = shared_y + i * 0.4
        window_rect = plt.Rectangle((0, window_y), shared_width, 0.3,
                                   facecolor='yellow', edgecolor='orange', linewidth=1, alpha=0.7)
        ax.add_patch(window_rect)
        
        window_label = f"{window['name']} ({window['size_mb']}MB, {window['access']})"
        ax.text(shared_width/2, window_y + 0.15, window_label,
               ha='center', va='center', fontsize=8)
    
    ax.set_xlim(-0.2, num_ranks * (rank_width + spacing))
    ax.set_ylim(-0.2, shared_y + len(architecture["shared_memory_windows"]) * 0.4 + 0.2)
    ax.set_aspect('equal')
    ax.axis('off')
    ax.set_title("MPI Multi-Process Architecture with Shared Memory", fontsize=14, weight='bold')
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    print(f"MPI architecture diagram saved to: {output_file}")

def generate_mpi_config(architecture: Dict, output_file: Path):
    """Generate MPI configuration file."""
    config = {
        "mpi": {
            "num_ranks": len(architecture["rank_assignments"]),
            "ranks": architecture["rank_assignments"],
            "shared_memory": {
                "windows": architecture["shared_memory_windows"],
                "implementation": "MPI-3"
            },
            "arena_allocation": architecture["arena_allocation"]
        }
    }
    
    with open(output_file, 'w') as f:
        json.dump(config, f, indent=2)
    
    print(f"MPI configuration saved to: {output_file}")

def main():
    parser = argparse.ArgumentParser(
        description="Analyze multiprocessing opportunities in OSPREY pipeline"
    )
    parser.add_argument("--config", type=Path, help="Pipeline configuration JSON")
    parser.add_argument("--output-dir", type=Path, default=Path("multiprocessing_analysis"),
                       help="Output directory")
    
    args = parser.parse_args()
    args.output_dir.mkdir(exist_ok=True)
    
    # Load or create default config
    if args.config:
        with open(args.config, 'r') as f:
            config = json.load(f)
    else:
        # Default configuration
        config = {
            "num_matches": 10,
            "num_sequences": 100,
            "num_ranks": 4,
            "threads_per_rank": 4
        }
        print("Using default configuration. Provide --config for custom settings.")
    
    # Analyze parallelism
    print("Analyzing parallelism opportunities...")
    
    kstar_config = {"num_sequences": config.get("num_sequences", 100)}
    sequence_analysis = analyze_sequence_parallelism(kstar_config)
    
    montage_config = {"num_matches": config.get("num_matches", 10)}
    match_analysis = analyze_match_parallelism(montage_config)
    
    # Design MPI architecture
    print("Designing MPI architecture...")
    mpi_architecture = design_mpi_architecture(config)
    
    # Generate outputs
    plot_mpi_architecture(mpi_architecture, args.output_dir / "mpi_architecture.png")
    generate_mpi_config(mpi_architecture, args.output_dir / "mpi_config.json")
    
    # Save analysis
    analysis = {
        "sequence_parallelism": sequence_analysis,
        "match_parallelism": match_analysis,
        "mpi_architecture": mpi_architecture
    }
    
    with open(args.output_dir / "analysis.json", 'w') as f:
        json.dump(analysis, f, indent=2)
    
    print(f"\nAnalysis complete! Results in: {args.output_dir}")

if __name__ == "__main__":
    main()

