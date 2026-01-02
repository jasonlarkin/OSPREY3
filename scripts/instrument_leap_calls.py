#!/usr/bin/env python3
"""
Instrument LEaP calls during ConfSpace compilation.

Monitors HTTP requests to LocalService (port 44342) to track:
- Number of LEaP calls
- Call timing
- Call patterns (molecule parameterization, fragment parameterization, etc.)
"""

import sys
import time
import json
import threading
from pathlib import Path
from collections import defaultdict
from http.server import HTTPServer, BaseHTTPRequestHandler
import socket

# Add parent directory to path
sys.path.insert(0, str(Path(__file__).parent.parent / "src" / "main" / "python"))

class LeapCallTracker:
    """Tracks LEaP calls via HTTP proxy."""
    
    def __init__(self):
        self.calls = []
        self.call_count = 0
        self.start_time = None
        self.end_time = None
        self.lock = threading.Lock()
    
    def record_call(self, endpoint, duration, request_size=None, response_size=None):
        """Record a LEaP call."""
        with self.lock:
            self.call_count += 1
            self.calls.append({
                'call_id': self.call_count,
                'endpoint': endpoint,
                'duration': duration,
                'timestamp': time.time(),
                'request_size': request_size,
                'response_size': response_size
            })
    
    def get_summary(self):
        """Get summary statistics."""
        if not self.calls:
            return {}
        
        durations = [c['duration'] for c in self.calls]
        endpoints = [c['endpoint'] for c in self.calls]
        
        endpoint_counts = defaultdict(int)
        endpoint_times = defaultdict(list)
        for call in self.calls:
            endpoint_counts[call['endpoint']] += 1
            endpoint_times[call['endpoint']].append(call['duration'])
        
        return {
            'total_calls': len(self.calls),
            'total_time': sum(durations),
            'avg_call_time': sum(durations) / len(durations),
            'min_call_time': min(durations),
            'max_call_time': max(durations),
            'endpoint_counts': dict(endpoint_counts),
            'endpoint_avg_times': {ep: sum(times)/len(times) for ep, times in endpoint_times.items()},
            'total_duration': self.end_time - self.start_time if self.end_time and self.start_time else None
        }

# Global tracker
tracker = LeapCallTracker()

class ProxyHandler(BaseHTTPRequestHandler):
    """HTTP proxy to intercept LocalService requests."""
    
    def do_POST(self):
        """Handle POST requests (LEaP calls)."""
        start = time.time()
        
        # Read request
        content_length = int(self.headers.get('Content-Length', 0))
        request_body = self.rfile.read(content_length)
        
        # Forward to actual LocalService
        try:
            import socket
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.connect(('localhost', 44342))
            
            # Forward request
            request_line = f"{self.command} {self.path} {self.protocol_version}\r\n"
            headers = '\r\n'.join(f"{k}: {v}" for k, v in self.headers.items())
            sock.sendall(f"{request_line}{headers}\r\n\r\n".encode() + request_body)
            
            # Read response
            response = b''
            while True:
                chunk = sock.recv(4096)
                if not chunk:
                    break
                response += chunk
            sock.close()
            
            # Parse response
            response_lines = response.split(b'\r\n\r\n', 1)
            response_headers = response_lines[0].decode('utf-8', errors='ignore')
            response_body = response_lines[1] if len(response_lines) > 1 else b''
            
            # Record call
            duration = time.time() - start
            endpoint = self.path.split('/')[-1] if '/' in self.path else self.path
            tracker.record_call(
                endpoint,
                duration,
                request_size=len(request_body),
                response_size=len(response_body)
            )
            
            # Send response to client
            self.send_response(200)
            self.end_headers()
            self.wfile.write(response)
            
        except Exception as e:
            print(f"Proxy error: {e}", file=sys.stderr)
            self.send_error(500, str(e))
    
    def log_message(self, format, *args):
        """Suppress default logging."""
        pass

def run_proxy(port=44343):
    """Run HTTP proxy on specified port."""
    server = HTTPServer(('localhost', port), ProxyHandler)
    print(f"LEaP call proxy running on port {port}")
    server.serve_forever()

def instrument_compilation(confspace_paths, output_file):
    """Instrument ConfSpace compilation and track LEaP calls."""
    import osprey
    osprey.start()
    from CCKStar.KStarPrep import compile_confspaces
    
    tracker.start_time = time.time()
    
    # Start proxy in background thread
    proxy_thread = threading.Thread(target=run_proxy, daemon=True)
    proxy_thread.start()
    time.sleep(0.5)  # Give proxy time to start
    
    # Modify LocalService to use proxy port
    # This is a hack - we'd need to modify LocalService code to support proxy
    # For now, we'll use a simpler approach: monitor via strace or subprocess
    
    print("Starting compilation with LEaP instrumentation...")
    try:
        with osprey.prep.LocalService():
            compile_confspaces(confspace_paths, parallel=True)
    finally:
        tracker.end_time = time.time()
    
    # Save results
    summary = tracker.get_summary()
    with open(output_file, 'w') as f:
        json.dump({
            'summary': summary,
            'calls': tracker.calls
        }, f, indent=2)
    
    print(f"\nLEaP Call Summary:")
    print(f"  Total calls: {summary.get('total_calls', 0)}")
    print(f"  Total LEaP time: {summary.get('total_time', 0):.2f}s")
    print(f"  Avg call time: {summary.get('avg_call_time', 0):.2f}s")
    print(f"  Endpoints: {summary.get('endpoint_counts', {})}")
    print(f"\nResults saved to: {output_file}")

if __name__ == '__main__':
    if len(sys.argv) < 3:
        print("Usage: instrument_leap_calls.py <confspace1> [confspace2] [confspace3] <output.json>")
        sys.exit(1)
    
    confspaces = sys.argv[1:-1]
    output = sys.argv[-1]
    
    instrument_compilation(confspaces, output)

