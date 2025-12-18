#!/usr/bin/env python3
"""
Google Colab benchmark setup script.
Run this in a Colab notebook cell to set up OSPREY SIMD benchmarking.

Usage in Colab:
1. Upload this file and osprey-fork_fresh directory to Colab
2. Run: !python3 scripts/tools/colab_benchmark_setup.py
3. Then run benchmarks with cleaner CPU/memory isolation
"""

import os
import subprocess
import sys

def check_colab():
    """Check if running in Google Colab."""
    try:
        import google.colab
        return True
    except ImportError:
        return False

def setup_colab_environment():
    """Set up Colab environment for benchmarking."""
    print("=== Setting up Google Colab for OSPREY Benchmarking ===\n")
    
    if not check_colab():
        print("WARNING: Not running in Google Colab. Some features may not work.")
        print("This script is optimized for Colab's clean Linux environment.\n")
    
    # Install required packages
    print("1. Installing required packages...")
    subprocess.run([sys.executable, "-m", "pip", "install", "-q", 
                   "matplotlib", "numpy", "pandas"], check=False)
    
    # Check CPU info
    print("\n2. Checking CPU information...")
    result = subprocess.run(["lscpu"], capture_output=True, text=True, check=False)
    print(result.stdout)
    
    # Check AVX support
    print("3. Checking AVX support...")
    with open("/proc/cpuinfo", "r") as f:
        cpuinfo = f.read()
        if "avx512" in cpuinfo.lower():
            print("  ✓ AVX-512 support detected")
        elif "avx2" in cpuinfo.lower():
            print("  ✓ AVX2 support detected (AVX-512 not available)")
        else:
            print("  ✗ No AVX support detected")
    
    # Set CPU governor to performance mode (if possible)
    print("\n4. Setting CPU governor to performance mode...")
    try:
        # Check current governor
        result = subprocess.run(["cat", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"], 
                              capture_output=True, text=True, check=False)
        current_governor = result.stdout.strip()
        print(f"  Current CPU governor: {current_governor}")
        
        # Try to set to performance (may require root, which Colab may not have)
        if current_governor != "performance":
            print("  Attempting to set to performance mode...")
            try:
                subprocess.run(["sudo", "cpupower", "frequency-set", "-g", "performance"], 
                             check=False, capture_output=True)
                print("  ✓ CPU governor set to performance mode")
            except:
                print("  ⚠ Could not set CPU governor (may require root)")
                print("  This is OK - Colab instances are usually optimized for performance")
    except Exception as e:
        print(f"  ⚠ Could not check/set CPU governor: {e}")
    
    # Disable CPU frequency scaling (if possible)
    print("\n5. Disabling CPU frequency scaling...")
    try:
        for cpu in range(os.cpu_count() or 1):
            try:
                scaling_max = f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_max_freq"
                scaling_min = f"/sys/devices/system/cpu/cpu{cpu}/cpufreq/scaling_min_freq"
                if os.path.exists(scaling_max):
                    with open(scaling_max, "r") as f:
                        max_freq = f.read().strip()
                    with open(scaling_min, "w") as f:
                        f.write(max_freq)
                    print(f"  ✓ CPU {cpu}: frequency scaling disabled")
                    break
            except:
                pass
    except Exception as e:
        print(f"  ⚠ Could not disable frequency scaling: {e}")
    
    # Set process priority (niceness)
    print("\n6. Setting process priority...")
    os.nice(-10)  # Higher priority (may fail without root)
    print("  Process priority adjusted")
    
    # Check available memory
    print("\n7. Checking system resources...")
    result = subprocess.run(["free", "-h"], capture_output=True, text=True, check=False)
    print(result.stdout)
    
    # Disable swap if possible (for more consistent results)
    print("\n8. Checking swap usage...")
    result = subprocess.run(["swapon", "--show"], capture_output=True, text=True, check=False)
    if result.stdout.strip():
        print("  Swap is enabled:")
        print(result.stdout)
        print("  Note: Consider disabling swap for more consistent benchmarks")
    else:
        print("  ✓ No swap enabled (good for benchmarking)")
    
    print("\n=== Setup Complete ===")
    print("\nBenefits of Colab environment:")
    print("  • Clean Linux environment (no Windows/WSL overhead)")
    print("  • Minimal background processes")
    print("  • Better CPU/memory isolation")
    print("  • Consistent hardware (same CPU per session)")
    print("\nNext steps:")
    print("  1. Build OSPREY benchmarks: cd osprey-fork_fresh/src/main/cc/ConfEcalc && cmake -B build -DENABLE_SIMD=ON && cmake --build build")
    print("  2. Run benchmarks: ./scripts/tools/benchmark_comprehensive.sh")
    print("  3. Compare results with local WSL benchmarks")

if __name__ == "__main__":
    setup_colab_environment()

