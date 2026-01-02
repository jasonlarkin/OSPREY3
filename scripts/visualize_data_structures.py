#!/usr/bin/env python3
"""
Visualize data structure lifetimes and relationships in OSPREY pipeline.

Creates diagrams showing:
- Data structure creation/destruction timeline
- Memory allocation patterns
- Object relationships (A* tree nodes, partition functions, etc.)
- Resource-lifetime boundaries
"""

import json
import argparse
from pathlib import Path
from typing import Dict, List, Optional
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from matplotlib.patches import Rectangle, FancyBboxPatch
import numpy as np

class DataStructure:
    """Represents a data structure with lifetime information."""
    def __init__(self, name: str, stage: str, created_at: float, destroyed_at: Optional[float] = None):
        self.name = name
        self.stage = stage
        self.created_at = created_at
        self.destroyed_at = destroyed_at
        self.size_mb = 0
        self.children = []
        self.parent = None

def parse_kstar_trace(trace_file: Path) -> List[DataStructure]:
    """Parse K* algorithm execution trace."""
    structures = []
    
    # Example trace format (would come from instrumentation)
    # Format: timestamp,event,structure_name,size_mb
    # Events: CREATE, DESTROY, RESET
    
    try:
        with open(trace_file, 'r') as f:
            for line in f:
                parts = line.strip().split(',')
                if len(parts) >= 3:
                    timestamp = float(parts[0])
                    event = parts[1]
                    name = parts[2]
                    size = float(parts[3]) if len(parts) > 3 else 0
                    
                    if event == "CREATE":
                        ds = DataStructure(name, "K*", timestamp)
                        ds.size_mb = size
                        structures.append(ds)
                    elif event == "DESTROY":
                        # Find matching structure
                        for s in structures:
                            if s.name == name and s.destroyed_at is None:
                                s.destroyed_at = timestamp
                                break
    except FileNotFoundError:
        print(f"Trace file not found: {trace_file}")
        print("Creating example trace for visualization...")
        # Create example data
        structures = create_example_kstar_trace()
    
    return structures

def create_example_kstar_trace() -> List[DataStructure]:
    """Create example K* trace based on known patterns."""
    structures = []
    
    # Sequence 1
    t = 0.0
    emat = DataStructure("EnergyMatrix", "K*", t)
    emat.size_mb = 500
    structures.append(emat)
    
    t += 0.1
    seq1_arena = DataStructure("Sequence1_Scratch", "K*", t)
    seq1_arena.size_mb = 10
    structures.append(seq1_arena)
    
    t += 0.01
    astar_tree = DataStructure("AStarTree_Seq1", "K*", t)
    astar_tree.size_mb = 200
    astar_tree.parent = seq1_arena
    seq1_arena.children.append(astar_tree)
    structures.append(astar_tree)
    
    t += 0.5
    pfunc = DataStructure("PartitionFunction_Seq1", "K*", t)
    pfunc.size_mb = 50
    pfunc.parent = seq1_arena
    seq1_arena.children.append(pfunc)
    structures.append(pfunc)
    
    t += 2.0
    seq1_arena.destroyed_at = t
    astar_tree.destroyed_at = t
    pfunc.destroyed_at = t
    
    # Sequence 2 (after forced GC)
    t += 0.01  # GC pause
    seq2_arena = DataStructure("Sequence2_Scratch", "K*", t)
    seq2_arena.size_mb = 10
    structures.append(seq2_arena)
    
    t += 0.01
    astar_tree2 = DataStructure("AStarTree_Seq2", "K*", t)
    astar_tree2.size_mb = 200
    astar_tree2.parent = seq2_arena
    seq2_arena.children.append(astar_tree2)
    structures.append(astar_tree2)
    
    t += 0.5
    pfunc2 = DataStructure("PartitionFunction_Seq2", "K*", t)
    pfunc2.size_mb = 50
    pfunc2.parent = seq2_arena
    seq2_arena.children.append(pfunc2)
    structures.append(pfunc2)
    
    t += 2.0
    seq2_arena.destroyed_at = t
    astar_tree2.destroyed_at = t
    pfunc2.destroyed_at = t
    
    return structures

def plot_lifetime_timeline(structures: List[DataStructure], output_file: Path):
    """Plot data structure lifetimes on a timeline."""
    fig, ax = plt.subplots(figsize=(16, 8))
    
    # Group by stage
    stages = {}
    for s in structures:
        if s.stage not in stages:
            stages[s.stage] = []
        stages[s.stage].append(s)
    
    # Color map
    colors = {
        "SCOPE": "#4dabf7",
        "MONTAGE": "#ff922b",
        "K*": "#51cf66",
        "Energy Matrix": "#845ef7"
    }
    
    y_pos = 0
    y_labels = []
    y_positions = []
    
    for stage in ["SCOPE", "MONTAGE", "Energy Matrix", "K*"]:
        if stage not in stages:
            continue
        
        stage_structures = stages[stage]
        stage_structures.sort(key=lambda x: x.created_at)
        
        for i, ds in enumerate(stage_structures):
            if ds.destroyed_at is None:
                duration = 1.0  # Default if not destroyed
            else:
                duration = ds.destroyed_at - ds.created_at
            
            # Draw lifetime bar
            color = colors.get(stage, "#cccccc")
            width = duration
            left = ds.created_at
            
            # Height proportional to size
            height = max(0.3, min(1.0, ds.size_mb / 100))
            
            rect = Rectangle((left, y_pos - height/2), width, height,
                            facecolor=color, edgecolor='black', alpha=0.7)
            ax.add_patch(rect)
            
            # Label
            if width > 0.1:  # Only label if wide enough
                ax.text(left + width/2, y_pos, ds.name,
                       ha='center', va='center', fontsize=8, rotation=0)
            
            y_positions.append(y_pos)
            y_labels.append(ds.name)
            y_pos += 1.5
    
    ax.set_xlabel("Time (seconds)")
    ax.set_ylabel("Data Structures")
    ax.set_title("Data Structure Lifetime Timeline")
    ax.set_yticks(y_positions)
    ax.set_yticklabels(y_labels, fontsize=7)
    ax.grid(True, alpha=0.3, axis='x')
    
    # Legend
    patches = [mpatches.Patch(color=colors[s], label=s) for s in stages.keys()]
    ax.legend(handles=patches, loc='upper right')
    
    # Adjust layout to prevent warnings
    plt.subplots_adjust(left=0.15, right=0.95, top=0.95, bottom=0.1)
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Lifetime timeline saved to: {output_file}")

def plot_allocation_boundaries(structures: List[DataStructure], output_file: Path):
    """Plot resource-lifetime boundaries (allocation churn reduction; approach TBD)."""
    fig, ax = plt.subplots(figsize=(16, 10))
    
    # Find scratch resources in the synthetic example trace
    arenas = [s for s in structures if "Scratch" in s.name]
    
    if not arenas:
        print("No boundary structures found, creating example...")
        # Create example arenas
        arenas = [s for s in structures if s.parent is None]
    
    y_pos = 0
    y_labels = []
    y_positions = []
    
    for boundary in sorted(arenas, key=lambda x: x.created_at):
        # Draw boundary
        if boundary.destroyed_at:
            duration = boundary.destroyed_at - boundary.created_at
        else:
            duration = 1.0
        
        # Boundary box
        arena_rect = FancyBboxPatch(
            (boundary.created_at, y_pos - 0.4), duration, 0.8,
            boxstyle="round,pad=0.1",
            facecolor='lightblue', edgecolor='blue', linewidth=2,
            alpha=0.3
        )
        ax.add_patch(arena_rect)
        
        # Label boundary
        ax.text(boundary.created_at + duration/2, y_pos, boundary.name,
               ha='center', va='center', fontsize=10, weight='bold')
        
        # Draw children
        children = [s for s in structures if s.parent == boundary]
        for i, child in enumerate(children):
            child_y = y_pos - 0.6 - i * 0.3
            
            if child.destroyed_at:
                child_duration = child.destroyed_at - child.created_at
            else:
                child_duration = duration
            
            child_rect = Rectangle(
                (child.created_at, child_y - 0.1), child_duration, 0.2,
                facecolor='lightgreen', edgecolor='green', alpha=0.7
            )
            ax.add_patch(child_rect)
            
            ax.text(child.created_at + child_duration/2, child_y, child.name,
                   ha='center', va='center', fontsize=8)
        
        y_positions.append(y_pos)
        y_labels.append(boundary.name)
        y_pos += 2.0
    
    ax.set_xlabel("Time (seconds)")
    ax.set_ylabel("Resource-Lifetime Boundaries")
    ax.set_title("Resource-Lifetime Boundaries (Per-Sequence Scratch)")
    ax.set_yticks(y_positions)
    ax.set_yticklabels(y_labels)
    ax.grid(True, alpha=0.3, axis='x')
    
    # Add legend
    arena_patch = mpatches.Patch(color='lightblue', label='Lifetime Boundary')
    child_patch = mpatches.Patch(color='lightgreen', label='Associated Object')
    ax.legend(handles=[arena_patch, child_patch], loc='upper right')
    
    # Adjust layout to prevent warnings
    plt.subplots_adjust(left=0.15, right=0.95, top=0.95, bottom=0.1)
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"Boundaries plot saved to: {output_file}")

def plot_memory_fragmentation(structures: List[DataStructure], output_file: Path):
    """Plot memory fragmentation patterns showing allocation/deallocation."""
    fig, (ax1, ax2) = plt.subplots(2, 1, figsize=(16, 10))
    
    # Timeline of allocations and deallocations
    alloc_times = []
    dealloc_times = []
    alloc_sizes = []
    dealloc_sizes = []
    
    for s in structures:
        alloc_times.append(s.created_at)
        alloc_sizes.append(s.size_mb)
        if s.destroyed_at:
            dealloc_times.append(s.destroyed_at)
            dealloc_sizes.append(s.size_mb)
    
    # Plot allocations
    ax1.scatter(alloc_times, alloc_sizes, c='green', alpha=0.6, s=50, label='Allocations')
    ax1.scatter(dealloc_times, dealloc_sizes, c='red', alpha=0.6, s=50, label='Deallocations')
    ax1.set_xlabel("Time (seconds)")
    ax1.set_ylabel("Size (MB)")
    ax1.set_title("Memory Allocation/Deallocation Timeline")
    ax1.legend()
    ax1.grid(True, alpha=0.3)
    
    # Cumulative memory usage
    times = sorted(set(alloc_times + dealloc_times))
    cumulative = []
    current = 0
    
    for t in times:
        # Add allocations at this time
        for i, at in enumerate(alloc_times):
            if abs(at - t) < 0.001:
                current += alloc_sizes[i]
        # Remove deallocations at this time
        for i, dt in enumerate(dealloc_times):
            if abs(dt - t) < 0.001:
                current -= dealloc_sizes[i]
        cumulative.append(current)
    
    ax2.plot(times, cumulative, linewidth=2, color='blue')
    ax2.fill_between(times, 0, cumulative, alpha=0.3, color='blue')
    ax2.set_xlabel("Time (seconds)")
    ax2.set_ylabel("Cumulative Memory (MB)")
    ax2.set_title("Cumulative Memory Usage")
    ax2.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    print(f"Memory fragmentation plot saved to: {output_file}")

def main():
    parser = argparse.ArgumentParser(
        description="Visualize OSPREY data structure lifetimes and resource-lifetime boundaries"
    )
    parser.add_argument("--trace-file", type=Path, help="K* execution trace file")
    parser.add_argument("--output-dir", type=Path, default=Path("visualizations"),
                       help="Output directory for visualizations")
    
    args = parser.parse_args()
    
    args.output_dir.mkdir(exist_ok=True)
    
    # Parse trace or create example
    if args.trace_file:
        structures = parse_kstar_trace(args.trace_file)
    else:
        print("No trace file provided, using example data...")
        structures = create_example_kstar_trace()
    
    # Generate visualizations
    print(f"Visualizing {len(structures)} data structures...")
    
    plot_lifetime_timeline(structures, args.output_dir / "lifetime_timeline.png")
    plot_allocation_boundaries(structures, args.output_dir / "allocation_boundaries.png")
    plot_memory_fragmentation(structures, args.output_dir / "memory_fragmentation.png")
    
    print(f"\nVisualizations saved to: {args.output_dir}")

if __name__ == "__main__":
    main()

