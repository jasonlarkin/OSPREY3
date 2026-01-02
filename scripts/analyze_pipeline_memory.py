#!/usr/bin/env python3
"""
Analyze memory allocation patterns in OSPREY pipeline to identify allocation-churn reduction opportunities.

Uses perf, valgrind, or JVM profiling to understand:
- Memory allocation patterns per pipeline stage
- Data structure lifetimes
- GC pressure points
- Memory fragmentation
- Allocation-churn reduction opportunities

Outputs:
- Memory allocation timeline plots
- Data structure lifetime diagrams
- GC pause analysis
- Allocation opportunity reports
"""

import subprocess
import sys
import json
import argparse
from pathlib import Path
from typing import Dict, List, Optional, Tuple
import matplotlib.pyplot as plt
import matplotlib.patches as mpatches
from collections import defaultdict
import numpy as np
import re
from datetime import datetime

def run_perf_mem_profile(command: List[str], output_file: Path) -> bool:
    """Run perf mem profiling on a command."""
    try:
        perf_cmd = [
            "perf", "mem", "record",
            "-o", str(output_file),
            "--"
        ] + command
        
        print(f"Running: {' '.join(perf_cmd)}")
        result = subprocess.run(perf_cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"perf mem failed: {result.stderr}")
            return False
        
        return True
    except FileNotFoundError:
        print("perf not found. Install with: sudo apt-get install linux-perf")
        return False

def run_valgrind_massif(command: List[str], output_file: Path) -> bool:
    """Run valgrind massif memory profiler."""
    try:
        valgrind_cmd = [
            "valgrind",
            "--tool=massif",
            f"--massif-out-file={output_file}",
            "--"
        ] + command
        
        print(f"Running: {' '.join(valgrind_cmd)}")
        result = subprocess.run(valgrind_cmd, capture_output=True, text=True)
        
        if result.returncode != 0:
            print(f"valgrind massif failed: {result.stderr}")
            return False
        
        return True
    except FileNotFoundError:
        print("valgrind not found. Install with: sudo apt-get install valgrind")
        return False

def parse_massif_output(massif_file: Path) -> Dict:
    """Parse valgrind massif output file."""
    data = {
        "snapshots": [],
        "peak_snapshot": None,
        "peak_memory": 0
    }
    
    try:
        with open(massif_file, 'r') as f:
            lines = f.readlines()
        
        current_snapshot = None
        for i, line in enumerate(lines):
            if line.startswith("snapshot="):
                if current_snapshot:
                    data["snapshots"].append(current_snapshot)
                current_snapshot = {
                    "snapshot_num": int(line.split("=")[1].strip()),
                    "time": 0,
                    "mem_heap": 0,
                    "mem_heap_extra": 0,
                    "mem_stacks": 0
                }
            elif line.startswith("time=") and current_snapshot:
                current_snapshot["time"] = int(line.split("=")[1].strip())
            elif line.startswith("mem_heap_B=") and current_snapshot:
                current_snapshot["mem_heap"] = int(line.split("=")[1].strip())
            elif line.startswith("mem_heap_extra_B=") and current_snapshot:
                current_snapshot["mem_heap_extra"] = int(line.split("=")[1].strip())
            elif line.startswith("mem_stacks_B=") and current_snapshot:
                current_snapshot["mem_stacks"] = int(line.split("=")[1].strip())
        
        if current_snapshot:
            data["snapshots"].append(current_snapshot)
        
        # Find peak
        for snap in data["snapshots"]:
            total = snap["mem_heap"] + snap["mem_heap_extra"] + snap["mem_stacks"]
            if total > data["peak_memory"]:
                data["peak_memory"] = total
                data["peak_snapshot"] = snap
        
    except Exception as e:
        print(f"Error parsing massif file: {e}")
    
    return data

def plot_memory_timeline(massif_data: Dict, output_file: Path):
    """Plot memory usage over time."""
    if not massif_data["snapshots"]:
        print("No snapshot data to plot")
        return
    
    times = [s["time"] / 1e6 for s in massif_data["snapshots"]]  # Convert to seconds
    heap = [s["mem_heap"] / 1e6 for s in massif_data["snapshots"]]  # Convert to MB
    heap_extra = [s["mem_heap_extra"] / 1e6 for s in massif_data["snapshots"]]
    stacks = [s["mem_stacks"] / 1e6 for s in massif_data["snapshots"]]
    
    fig, ax = plt.subplots(figsize=(12, 6))
    
    ax.fill_between(times, 0, heap, label="Heap", alpha=0.7)
    ax.fill_between(times, heap, [h + he for h, he in zip(heap, heap_extra)], 
                    label="Heap Extra", alpha=0.7)
    ax.fill_between(times, [h + he for h, he in zip(heap, heap_extra)],
                    [h + he + s for h, he, s in zip(heap, heap_extra, stacks)],
                    label="Stacks", alpha=0.7)
    
    # Mark peak
    peak_time = massif_data["peak_snapshot"]["time"] / 1e6
    peak_mem = massif_data["peak_memory"] / 1e6
    ax.axvline(peak_time, color='red', linestyle='--', label=f'Peak: {peak_mem:.1f} MB')
    
    ax.set_xlabel("Time (seconds)")
    ax.set_ylabel("Memory (MB)")
    ax.set_title("Memory Usage Timeline")
    ax.legend()
    ax.grid(True, alpha=0.3)
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300)
    print(f"Memory timeline plot saved to: {output_file}")

def analyze_kstar_memory_patterns(java_pid: Optional[int] = None) -> Dict:
    """Analyze K* algorithm memory patterns using JVM profiling."""
    analysis = {
        "gc_events": [],
        "allocation_sites": [],
        "object_lifetimes": []
    }
    
    if java_pid:
        # Use async-profiler or JVM profiling
        try:
            # Try async-profiler if available
            cmd = [
                "java", "-jar", "async-profiler.jar",
                "-e", "alloc",
                "-d", "30",
                "-f", "kstar_alloc_profile.html",
                str(java_pid)
            ]
            subprocess.run(cmd, check=True)
            print("Allocation profile generated: kstar_alloc_profile.html")
        except:
            print("async-profiler not available, skipping allocation profiling")
    
    return analysis

def identify_allocation_opportunities(massif_data: Dict, stage_info: Dict) -> List[Dict]:
    """Identify opportunities for reducing allocation churn based on memory patterns."""
    opportunities = []
    
    # Analyze memory growth patterns
    if massif_data["snapshots"]:
        snapshots = massif_data["snapshots"]
        
        # Look for rapid allocation followed by deallocation (per-sequence pattern)
        for i in range(1, len(snapshots)):
            prev = snapshots[i-1]
            curr = snapshots[i]
            
            heap_growth = curr["mem_heap"] - prev["mem_heap"]
            time_delta = (curr["time"] - prev["time"]) / 1e6  # seconds
            
            if heap_growth > 10 * 1024 * 1024 and time_delta < 1.0:  # >10MB in <1s
                opportunities.append({
                    "type": "rapid_allocation",
                    "stage": stage_info.get("current_stage", "unknown"),
                    "time": curr["time"] / 1e6,
                    "heap_growth_mb": heap_growth / 1e6,
                    "rate_mb_per_s": (heap_growth / 1e6) / time_delta if time_delta > 0 else 0,
                    "recommendation": "Per-sequence scratch allocation strategy (approach TBD)"
                })
    
    # Check for forced GC patterns (evidence from K* code)
    if stage_info.get("has_forced_gc", False):
        opportunities.append({
            "type": "forced_gc_hack",
            "stage": "K* Algorithm",
            "location": "KStar.java:374-382",
            "recommendation": "Per-sequence scratch allocation strategy to reduce forced GC (approach TBD)",
            "impact": "1+ seconds overhead for 100 sequences"
        })
    
    return opportunities

def parse_gc_log(gc_log_file: Path) -> Dict:
    """Parse Java 17 unified GC log format with ISO timestamps."""
    gc_data = {
        "events": [],
        "heap_sizes": [],
        "gc_pauses": [],
        "young_gc_count": 0,
        "full_gc_count": 0,
        "total_pause_time": 0.0,
        "start_time": None,
        "end_time": None,
        "first_timestamp": None
    }
    
    # Pattern for ISO timestamp: [2025-12-25T04:09:41.971-0500]
    iso_timestamp_pattern = re.compile(
        r'\[(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}-\d{4})\]'
    )
    
    # Pattern for GC summary line: GC(0) Pause Young (Normal) (G1 Evacuation Pause) 16M->10M(124M) 116.562ms
    # Match: GC ID, type (everything until heap sizes), heap sizes, pause time
    # Use lookahead to match everything up to the heap size pattern
    gc_summary_pattern = re.compile(
        r'GC\((\d+)\)\s+Pause\s+(.+?)(?=\s+\d+M->)\s+(\d+)M->(\d+)M\((\d+)M\)\s+(\d+\.?\d*)\s*ms'
    )
    
    # Alternative pattern without heap info: GC(0) Pause Young (Normal) 116.562ms
    gc_simple_pattern = re.compile(
        r'GC\((\d+)\)\s+Pause\s+([^(]+?)\s+(?:\([^)]+\)\s+)?(\d+\.?\d*)\s*ms'
    )
    
    def parse_iso_timestamp(ts_str: str) -> float:
        """Parse ISO timestamp and return seconds since epoch."""
        try:
            # Format: 2025-12-25T04:09:41.971-0500
            dt = datetime.strptime(ts_str[:-5], "%Y-%m-%dT%H:%M:%S.%f")
            # Note: We ignore timezone offset for relative timing
            return dt.timestamp()
        except:
            return 0.0
    
    try:
        with open(gc_log_file, 'r', encoding='utf-8', errors='ignore') as f:
            for line in f:
                line = line.strip()
                if not line:
                    continue
                
                # Extract timestamp
                ts_match = iso_timestamp_pattern.match(line)
                if not ts_match:
                    continue
                
                timestamp_str = ts_match.group(1)
                timestamp = parse_iso_timestamp(timestamp_str)
                
                # Set first timestamp as reference
                if gc_data["first_timestamp"] is None:
                    gc_data["first_timestamp"] = timestamp
                    gc_data["start_time"] = 0.0
                
                # Convert to relative time (seconds since start)
                relative_time = timestamp - gc_data["first_timestamp"]
                gc_data["end_time"] = relative_time
                
                # Check if this is a GC summary line (has [gc] tag but not [gc,start] or [gc,phases])
                # Match lines like: [timestamp][gc          ] GC(0) Pause Young (Normal) (G1 Evacuation Pause) 16M->10M(124M) 116.562ms
                # The [gc] tag may have spaces: [gc          ]
                if re.search(r'\[gc\s*\]', line) and '[gc,start]' not in line and '[gc,phases]' not in line and '[gc,heap]' not in line and '[gc,task]' not in line and '[gc,cpu]' not in line and '[gc,metaspace]' not in line:
                    # Try full pattern with heap sizes
                    match = gc_summary_pattern.search(line)
                    if match:
                        gc_id = int(match.group(1))
                        gc_type_full = match.group(2).strip()
                        # Extract just the type name (e.g., "Young" from "Young (Normal) (G1 Evacuation Pause)")
                        gc_type = gc_type_full.split()[0] if gc_type_full else "Unknown"
                        before_mb = int(match.group(3))
                        after_mb = int(match.group(4))
                        total_mb = int(match.group(5))
                        pause_time_ms = float(match.group(6))
                        pause_time = pause_time_ms / 1000.0  # Convert to seconds
                        
                        event = {
                            "timestamp": relative_time,
                            "gc_id": gc_id,
                            "type": gc_type,
                            "pause_time": pause_time,
                            "before_mb": before_mb,
                            "after_mb": after_mb,
                            "total_mb": total_mb,
                            "message": line
                        }
                        gc_data["events"].append(event)
                        gc_data["gc_pauses"].append(pause_time)
                        gc_data["total_pause_time"] += pause_time
                        
                        gc_data["heap_sizes"].append({
                            "timestamp": relative_time,
                            "before_mb": before_mb,
                            "after_mb": after_mb,
                            "total_mb": total_mb
                        })
                        
                        if "Young" in gc_type or "Normal" in gc_type:
                            gc_data["young_gc_count"] += 1
                        elif "Full" in gc_type:
                            gc_data["full_gc_count"] += 1
                    else:
                        # Try simple pattern without heap info
                        match = gc_simple_pattern.search(line)
                        if match:
                            gc_id = int(match.group(1))
                            gc_type = match.group(2).strip()
                            pause_time_ms = float(match.group(3))
                            pause_time = pause_time_ms / 1000.0
                            
                            event = {
                                "timestamp": relative_time,
                                "gc_id": gc_id,
                                "type": gc_type,
                                "pause_time": pause_time,
                                "message": line
                            }
                            gc_data["events"].append(event)
                            gc_data["gc_pauses"].append(pause_time)
                            gc_data["total_pause_time"] += pause_time
                            
                            if "Young" in gc_type or "Normal" in gc_type:
                                gc_data["young_gc_count"] += 1
                            elif "Full" in gc_type:
                                gc_data["full_gc_count"] += 1
    
    except Exception as e:
        print(f"Error parsing GC log: {e}")
        import traceback
        traceback.print_exc()
    
    return gc_data

def plot_gc_analysis(gc_data: Dict, output_dir: Path):
    """Plot GC analysis charts."""
    if not gc_data["events"]:
        print("No GC events found in log")
        return
    
    # Create figure with subplots
    fig, axes = plt.subplots(2, 2, figsize=(14, 10))
    
    # 1. GC Pause Times Over Time
    ax1 = axes[0, 0]
    timestamps = [e["timestamp"] for e in gc_data["events"]]
    pause_times = [e["pause_time"] * 1000 for e in gc_data["events"]]  # Convert to ms
    colors = ['red' if 'Full' in e["type"] else 'blue' for e in gc_data["events"]]
    
    ax1.scatter(timestamps, pause_times, c=colors, alpha=0.6, s=20)
    ax1.set_xlabel("Time (seconds)")
    ax1.set_ylabel("GC Pause Time (ms)")
    ax1.set_title("GC Pause Times Over Time")
    ax1.grid(True, alpha=0.3)
    # Create custom legend for GC types
    from matplotlib.lines import Line2D
    legend_elements = [
        Line2D([0], [0], marker='o', color='w', markerfacecolor='blue', markersize=8, label='Young GC'),
        Line2D([0], [0], marker='o', color='w', markerfacecolor='red', markersize=8, label='Full GC')
    ]
    ax1.legend(handles=legend_elements)
    
    # 2. Heap Size Over Time
    ax2 = axes[0, 1]
    if gc_data["heap_sizes"]:
        heap_times = [h["timestamp"] for h in gc_data["heap_sizes"]]
        heap_before = [h["before_mb"] for h in gc_data["heap_sizes"]]
        heap_after = [h["after_mb"] for h in gc_data["heap_sizes"]]
        heap_total = [h["total_mb"] for h in gc_data["heap_sizes"]]
        
        ax2.plot(heap_times, heap_before, 'o-', label='Before GC', alpha=0.7, markersize=3)
        ax2.plot(heap_times, heap_after, 's-', label='After GC', alpha=0.7, markersize=3)
        ax2.plot(heap_times, heap_total, '--', label='Total Heap', alpha=0.5)
        ax2.set_xlabel("Time (seconds)")
        ax2.set_ylabel("Heap Size (MB)")
        ax2.set_title("Heap Size Over Time")
        ax2.legend()
        ax2.grid(True, alpha=0.3)
    else:
        ax2.text(0.5, 0.5, "No heap size data", ha='center', va='center', transform=ax2.transAxes)
        ax2.set_title("Heap Size Over Time (No Data)")
    
    # 3. GC Frequency Histogram
    ax3 = axes[1, 0]
    if len(gc_data["gc_pauses"]) > 1:
        # Calculate time between GCs
        gc_intervals = []
        for i in range(1, len(gc_data["events"])):
            interval = gc_data["events"][i]["timestamp"] - gc_data["events"][i-1]["timestamp"]
            gc_intervals.append(interval)
        
        if gc_intervals:
            ax3.hist(gc_intervals, bins=min(30, len(gc_intervals)), edgecolor='black', alpha=0.7)
            ax3.set_xlabel("Time Between GCs (seconds)")
            ax3.set_ylabel("Frequency")
            ax3.set_title("GC Frequency Distribution")
            ax3.grid(True, alpha=0.3)
    else:
        ax3.text(0.5, 0.5, "Insufficient data", ha='center', va='center', transform=ax3.transAxes)
        ax3.set_title("GC Frequency Distribution")
    
    # 4. GC Pause Time Distribution
    ax4 = axes[1, 1]
    if gc_data["gc_pauses"]:
        pause_times_ms = [p * 1000 for p in gc_data["gc_pauses"]]
        ax4.hist(pause_times_ms, bins=min(30, len(pause_times_ms)), edgecolor='black', alpha=0.7)
        ax4.set_xlabel("GC Pause Time (ms)")
        ax4.set_ylabel("Frequency")
        ax4.set_title("GC Pause Time Distribution")
        ax4.grid(True, alpha=0.3)
    else:
        ax4.text(0.5, 0.5, "No pause data", ha='center', va='center', transform=ax4.transAxes)
        ax4.set_title("GC Pause Time Distribution")
    
    plt.tight_layout()
    
    plot_file = output_dir / "gc_analysis.png"
    plt.savefig(plot_file, dpi=300, bbox_inches='tight')
    print(f"GC analysis plot saved to: {plot_file}")

def generate_gc_report(gc_data: Dict, output_file: Path):
    """Generate a markdown report from GC analysis."""
    with open(output_file, 'w') as f:
        f.write("# GC Log Analysis Report\n\n")
        
        f.write("## Summary\n\n")
        f.write(f"- **Total GC Events**: {len(gc_data['events'])}\n")
        f.write(f"- **Young GC Count**: {gc_data['young_gc_count']}\n")
        f.write(f"- **Full GC Count**: {gc_data['full_gc_count']}\n")
        f.write(f"- **Total GC Pause Time**: {gc_data['total_pause_time']:.3f} seconds\n")
        
        if gc_data["gc_pauses"]:
            avg_pause = sum(gc_data["gc_pauses"]) / len(gc_data["gc_pauses"])
            max_pause = max(gc_data["gc_pauses"])
            f.write(f"- **Average GC Pause**: {avg_pause * 1000:.2f} ms\n")
            f.write(f"- **Maximum GC Pause**: {max_pause * 1000:.2f} ms\n")
        
        if gc_data["start_time"] is not None and gc_data["end_time"] is not None:
            duration = gc_data["end_time"] - gc_data["start_time"]
            f.write(f"- **Total Duration**: {duration:.2f} seconds\n")
            if duration > 0:
                gc_percent = (gc_data["total_pause_time"] / duration) * 100
                f.write(f"- **GC Overhead**: {gc_percent:.2f}%\n")
        
        f.write("\n## GC Event Timeline\n\n")
        f.write("| Time (s) | GC ID | Type | Pause (ms) | Heap Before->After (Total) |\n")
        f.write("|----------|-------|------|------------|---------------------------|\n")
        
        for event in gc_data["events"][:50]:  # Limit to first 50 for readability
            heap_info = ""
            if "before_mb" in event:
                heap_info = f"{event['before_mb']}M->{event['after_mb']}M({event['total_mb']}M)"
            else:
                heap_info = "N/A"
            f.write(f"| {event['timestamp']:.3f} | {event['gc_id']} | {event['type']} | {event['pause_time']*1000:.2f} | {heap_info} |\n")
        
        if len(gc_data["events"]) > 50:
            f.write(f"\n*... and {len(gc_data['events']) - 50} more events*\n")
    
    print(f"GC report saved to: {output_file}")

def identify_gc_allocation_opportunities(gc_data: Dict, stage_info: Dict) -> List[Dict]:
    """Identify allocation-churn reduction opportunities based on GC patterns."""
    opportunities = []
    
    # Check for frequent GCs (indicates allocation pressure)
    if len(gc_data["events"]) > 10:
        duration = gc_data["end_time"] - gc_data["start_time"] if gc_data["end_time"] else 1.0
        gc_rate = len(gc_data["events"]) / duration if duration > 0 else 0
        
        if gc_rate > 1.0:  # More than 1 GC per second
            opportunities.append({
                "type": "frequent_gc",
                "stage": stage_info.get("current_stage", "unknown"),
                "gc_count": len(gc_data["events"]),
                "gc_rate": gc_rate,
                "total_pause_time": gc_data["total_pause_time"],
                "avg_pause_time": gc_data["total_pause_time"] / len(gc_data["events"]) if gc_data["events"] else 0,
                "recommendation": f"Reduce allocation churn to reduce GC frequency (currently {gc_rate:.2f} GCs/sec)"
            })
    
    # Check for high GC overhead
    if gc_data["start_time"] is not None and gc_data["end_time"] is not None:
        duration = gc_data["end_time"] - gc_data["start_time"]
        if duration > 0:
            gc_overhead = (gc_data["total_pause_time"] / duration) * 100
            if gc_overhead > 5.0:  # More than 5% overhead
                opportunities.append({
                    "type": "high_gc_overhead",
                    "stage": stage_info.get("current_stage", "unknown"),
                    "gc_overhead_percent": gc_overhead,
                    "total_pause_time": gc_data["total_pause_time"],
                    "recommendation": f"Reduce allocation churn to reduce GC overhead (currently {gc_overhead:.2f}%)"
                })
    
    # Check for Full GCs (bad sign)
    if gc_data["full_gc_count"] > 0:
        opportunities.append({
            "type": "full_gc_detected",
            "stage": stage_info.get("current_stage", "unknown"),
            "full_gc_count": gc_data["full_gc_count"],
            "recommendation": "Reduce allocation churn to prevent Full GCs (indicates memory pressure)"
        })
    
    return opportunities

def generate_allocation_opportunity_report(opportunities: List[Dict], output_file: Path):
    """Generate a report of allocation-churn reduction opportunities."""
    with open(output_file, 'w') as f:
        f.write("# Allocation-Churn Reduction Opportunities\n\n")
        f.write(f"Found {len(opportunities)} opportunities\n\n")
        
        for i, opp in enumerate(opportunities, 1):
            f.write(f"## Opportunity {i}: {opp['type']}\n\n")
            f.write(f"- **Stage**: {opp.get('stage', 'unknown')}\n")
            if 'time' in opp:
                f.write(f"- **Time**: {opp['time']:.2f} seconds\n")
            if 'heap_growth_mb' in opp:
                f.write(f"- **Heap Growth**: {opp['heap_growth_mb']:.2f} MB\n")
            if 'rate_mb_per_s' in opp:
                f.write(f"- **Allocation Rate**: {opp['rate_mb_per_s']:.2f} MB/s\n")
            if 'location' in opp:
                f.write(f"- **Location**: {opp['location']}\n")
            if 'impact' in opp:
                f.write(f"- **Impact**: {opp['impact']}\n")
            f.write(f"- **Recommendation**: {opp['recommendation']}\n\n")
    
    print(f"Allocation opportunity report saved to: {output_file}")

def main():
    parser = argparse.ArgumentParser(
        description="Analyze OSPREY pipeline memory patterns for allocation-churn reduction opportunities"
    )
    parser.add_argument("--command", nargs="+", help="Command to profile (e.g., ./gradlew test)")
    parser.add_argument("--stage", choices=["scope", "montage", "kstar", "all"], 
                       default="all", help="Pipeline stage to analyze")
    parser.add_argument("--tool", choices=["perf", "valgrind", "jvm"], 
                       default="valgrind", help="Profiling tool to use")
    parser.add_argument("--output-dir", type=Path, default=Path("memory_analysis"),
                       help="Output directory for analysis results")
    parser.add_argument("--java-pid", type=int, help="Java process ID for JVM profiling")
    parser.add_argument("--massif-file", type=Path, help="Existing massif output file to analyze")
    parser.add_argument("--gc-log", type=Path, help="JVM GC log file to analyze")
    
    args = parser.parse_args()
    
    # Create output directory
    args.output_dir.mkdir(exist_ok=True)
    
    # Check for existing massif file first
    if args.massif_file and args.massif_file.exists():
        print(f"Analyzing existing massif file: {args.massif_file}")
        massif_data = parse_massif_output(args.massif_file)
        
        # Plot memory timeline
        plot_file = args.output_dir / "memory_timeline.png"
        plot_memory_timeline(massif_data, plot_file)
        
        # Identify allocation opportunities
        stage_info = {"current_stage": args.stage, "has_forced_gc": True}  # K* has forced GC
        opportunities = identify_allocation_opportunities(massif_data, stage_info)
        
        # Generate report
        report_file = args.output_dir / "allocation_opportunities.md"
        generate_allocation_opportunity_report(opportunities, report_file)
        
        print(f"\nAnalysis complete! Results in: {args.output_dir}")
        return
    
    if args.command:
        # Profile the command
        if args.tool == "valgrind":
            massif_file = args.output_dir / "massif.out"
            print(f"Profiling command: {' '.join(args.command)}")
            print("This may take several minutes...")
            
            if run_valgrind_massif(args.command, massif_file):
                massif_data = parse_massif_output(massif_file)
                
                # Plot memory timeline
                plot_file = args.output_dir / "memory_timeline.png"
                plot_memory_timeline(massif_data, plot_file)
                
                # Identify allocation opportunities
                stage_info = {"current_stage": args.stage, "has_forced_gc": True}
                opportunities = identify_allocation_opportunities(massif_data, stage_info)
                
                # Generate report
                report_file = args.output_dir / "allocation_opportunities.md"
                generate_allocation_opportunity_report(opportunities, report_file)
        
        elif args.tool == "perf":
            perf_file = args.output_dir / "perf.data"
            if run_perf_mem_profile(args.command, perf_file):
                print(f"perf data saved to: {perf_file}")
                print("Analyze with: perf mem report")
    
    if args.java_pid:
        # Analyze Java/K* memory patterns
        kstar_analysis = analyze_kstar_memory_patterns(args.java_pid)
        analysis_file = args.output_dir / "kstar_memory_analysis.json"
        with open(analysis_file, 'w') as f:
            json.dump(kstar_analysis, f, indent=2)
        print(f"K* memory analysis saved to: {analysis_file}")
    
    if args.gc_log and args.gc_log.exists():
        # Parse GC log
        print(f"Analyzing GC log: {args.gc_log}")
        gc_data = parse_gc_log(args.gc_log)
        
        # Plot GC analysis
        plot_gc_analysis(gc_data, args.output_dir)
        
        # Generate GC report
        generate_gc_report(gc_data, args.output_dir / "gc_analysis.md")
        
        # Identify allocation opportunities from GC patterns
        stage_info = {"current_stage": args.stage, "has_forced_gc": True}
        gc_opportunities = identify_gc_allocation_opportunities(gc_data, stage_info)
        
        # Add to existing opportunities or create new report
        if gc_opportunities:
            report_file = args.output_dir / "allocation_opportunities.md"
            if report_file.exists():
                # Append to existing report
                with open(report_file, 'a') as f:
                    f.write("\n\n## GC-Based Allocation Opportunities\n\n")
                    for opp in gc_opportunities:
                        f.write(f"### {opp['type']}\n\n")
                        f.write(f"- **Stage**: {opp.get('stage', 'unknown')}\n")
                        if 'gc_count' in opp:
                            f.write(f"- **GC Events**: {opp['gc_count']}\n")
                        if 'total_pause_time' in opp:
                            f.write(f"- **Total GC Pause Time**: {opp['total_pause_time']:.3f} seconds\n")
                        if 'avg_pause_time' in opp:
                            f.write(f"- **Average GC Pause**: {opp['avg_pause_time']:.3f} seconds\n")
                        if 'recommendation' in opp:
                            f.write(f"- **Recommendation**: {opp['recommendation']}\n")
                        f.write("\n")
            else:
                generate_allocation_opportunity_report(gc_opportunities, report_file)
    
    print("\nAnalysis complete!")
    print(f"Results in: {args.output_dir}")

if __name__ == "__main__":
    main()

