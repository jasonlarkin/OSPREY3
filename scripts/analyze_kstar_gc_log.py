#!/usr/bin/env python3
"""
Analyze GC logs from K* runs to extract workload characteristics.
"""
import sys
import re
import json
from pathlib import Path
from collections import defaultdict

def parse_gc_log(gc_log_path):
    """Parse JVM GC log and extract metrics."""
    if not gc_log_path.exists():
        return None
    
    events = []
    with open(gc_log_path) as f:
        for line in f:
            # Parse GC event
            # Format: [timestamp] GC(0) Pause Young (Allocation Failure) 16M->10M(124M) 116.56ms
            match = re.search(r'GC\((\d+)\)\s+(\w+)\s+(.*?)\s+(\d+)M->(\d+)M\((\d+)M\)\s+([\d.]+)ms', line)
            if match:
                gc_id, gc_type, reason, before, after, total, pause = match.groups()
                events.append({
                    'id': int(gc_id),
                    'type': gc_type,
                    'reason': reason,
                    'before_mb': int(before),
                    'after_mb': int(after),
                    'total_mb': int(total),
                    'pause_ms': float(pause)
                })
    
    if not events:
        return None
    
    # Calculate statistics
    young_gcs = [e for e in events if e['type'] == 'Young']
    full_gcs = [e for e in events if e['type'] == 'Full' or 'Full' in e['type']]
    
    total_pause = sum(e['pause_ms'] for e in events)
    peak_memory = max(e['total_mb'] for e in events)
    
    return {
        'total_events': len(events),
        'young_gc_count': len(young_gcs),
        'full_gc_count': len(full_gcs),
        'total_pause_ms': total_pause,
        'avg_pause_ms': total_pause / len(events) if events else 0,
        'max_pause_ms': max(e['pause_ms'] for e in events) if events else 0,
        'peak_memory_mb': peak_memory,
        'initial_memory_mb': events[0]['total_mb'] if events else 0,
        'final_memory_mb': events[-1]['total_mb'] if events else 0,
    }

def main():
    if len(sys.argv) < 2:
        print("Usage: analyze_kstar_gc_log.py <gc_log_file> [output_json]")
        sys.exit(1)
    
    gc_log_path = Path(sys.argv[1])
    output_json = sys.argv[2] if len(sys.argv) > 2 else None
    
    stats = parse_gc_log(gc_log_path)
    
    if stats is None:
        print(f"ERROR: Could not parse GC log: {gc_log_path}")
        sys.exit(1)
    
    # Print summary
    print("GC Log Analysis")
    print("=" * 60)
    print(f"Total GC Events: {stats['total_events']}")
    print(f"Young GC: {stats['young_gc_count']}")
    print(f"Full GC: {stats['full_gc_count']}")
    print(f"Total GC Pause: {stats['total_pause_ms']:.2f} ms")
    print(f"Average Pause: {stats['avg_pause_ms']:.2f} ms")
    print(f"Max Pause: {stats['max_pause_ms']:.2f} ms")
    print(f"Peak Memory: {stats['peak_memory_mb']} MB")
    print(f"Memory Growth: {stats['initial_memory_mb']} MB → {stats['final_memory_mb']} MB")
    
    # Write JSON if requested
    if output_json:
        with open(output_json, 'w') as f:
            json.dump(stats, f, indent=2)
        print(f"\nResults saved to: {output_json}")

if __name__ == '__main__':
    main()

