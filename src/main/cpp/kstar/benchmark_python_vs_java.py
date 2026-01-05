#!/usr/bin/env python3
"""
Benchmark C++ Python bindings vs Java implementation.

Compares computation time, memory usage, and results between C++ and Java K* implementations.
"""

import sys
import os
import time
import argparse

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
    import resource
    HAS_RESOURCE = True
except ImportError:
    HAS_RESOURCE = False


def get_memory_usage():
    """Get current memory usage in MB (Linux/Mac only)."""
    if not HAS_RESOURCE:
        return None
    try:
        usage = resource.getrusage(resource.RUSAGE_SELF)
        return usage.ru_maxrss / 1024.0  # Convert KB to MB (Linux) or pages to MB (Mac)
    except:
        return None


def benchmark_cpp(protein_file, ligand_file, complex_file, epsilon, num_runs=1):
    """Benchmark C++ implementation."""
    print(f"\n=== C++ Implementation ===")
    
    # Load energy matrices
    load_start = time.perf_counter()
    protein = kstar_cpp.load_energy_matrix(protein_file)
    ligand = kstar_cpp.load_energy_matrix(ligand_file)
    complex_emat = kstar_cpp.load_energy_matrix(complex_file)
    load_time = time.perf_counter() - load_start
    
    print(f"Load time: {load_time*1000:.2f} ms")
    
    # Memory before computation
    mem_before = get_memory_usage()
    
    # Warmup run
    workflow = kstar_cpp.KStarWorkflow()
    workflow.compute(protein, ligand, complex_emat, epsilon=epsilon)
    
    # Timed runs
    compute_times = []
    results = []
    
    for i in range(num_runs):
        compute_start = time.perf_counter()
        result = workflow.compute(protein, ligand, complex_emat, epsilon=epsilon)
        compute_time = time.perf_counter() - compute_start
        compute_times.append(compute_time)
        results.append(result)
    
    # Memory after computation
    mem_after = get_memory_usage()
    
    avg_time = sum(compute_times) / len(compute_times)
    min_time = min(compute_times)
    max_time = max(compute_times)
    
    print(f"Compute time (avg over {num_runs} runs): {avg_time:.3f} s")
    if num_runs > 1:
        print(f"  Min: {min_time:.3f} s, Max: {max_time:.3f} s")
    
    if mem_before is not None and mem_after is not None:
        mem_delta = mem_after - mem_before
        print(f"Memory usage: {mem_after:.1f} MB (delta: {mem_delta:+.1f} MB)")
    
    # Results from last run
    result = results[-1]
    print(f"\nResults:")
    print(f"  log10(K*) = {result.log10_value:.6f}")
    print(f"  Bounds: [{result.log10_lower_bound:.6f}, {result.log10_upper_bound:.6f}]")
    print(f"  Converged: {result.converged}")
    
    print(f"\nPartition Functions:")
    print(f"  Protein: log10(Q) = [{result.pfuncs.protein.lower_bound:.6f}, {result.pfuncs.protein.upper_bound:.6f}], "
          f"confs={result.pfuncs.protein.num_confs}, delta={result.pfuncs.protein.delta:.6f}")
    print(f"  Ligand: log10(Q) = [{result.pfuncs.ligand.lower_bound:.6f}, {result.pfuncs.ligand.upper_bound:.6f}], "
          f"confs={result.pfuncs.ligand.num_confs}, delta={result.pfuncs.ligand.delta:.6f}")
    print(f"  Complex: log10(Q) = [{result.pfuncs.complex.lower_bound:.6f}, {result.pfuncs.complex.upper_bound:.6f}], "
          f"confs={result.pfuncs.complex.num_confs}, delta={result.pfuncs.complex.delta:.6f}")
    
    return {
        'load_time': load_time,
        'compute_time': avg_time,
        'compute_times': compute_times,
        'memory_mb': mem_after,
        'result': result
    }


def benchmark_cpp_variants(protein_file, ligand_file, complex_file, epsilon):
    """Compare Baseline vs Fast A* variants."""
    print(f"\n=== C++ A* Variant Comparison ===")
    
    protein = kstar_cpp.load_energy_matrix(protein_file)
    ligand = kstar_cpp.load_energy_matrix(ligand_file)
    complex_emat = kstar_cpp.load_energy_matrix(complex_file)
    
    workflow = kstar_cpp.KStarWorkflow()
    
    variants = [
        ("Baseline", kstar_cpp.AStarVariant.Baseline),
        ("Fast", kstar_cpp.AStarVariant.Fast),
    ]
    
    results = {}
    
    for name, variant in variants:
        options = kstar_cpp.PartitionFunctionOptions()
        options.astar_variant = variant
        options.allow_exact_enumeration = False
        
        start = time.perf_counter()
        result = workflow.compute(protein, ligand, complex_emat, epsilon=epsilon,
                                  method=kstar_cpp.PartitionFunctionMethod.AStar,
                                  options=options)
        elapsed = time.perf_counter() - start
        
        results[name] = {
            'time': elapsed,
            'result': result
        }
        
        print(f"\n{name}:")
        print(f"  Time: {elapsed:.3f} s")
        print(f"  log10(K*) = {result.log10_value:.6f}")
        print(f"  Converged: {result.converged}")
    
    if len(results) == 2:
        baseline_time = results['Baseline']['time']
        fast_time = results['Fast']['time']
        speedup = baseline_time / fast_time
        print(f"\nSpeedup (Fast vs Baseline): {speedup:.2f}x")
        
        # Verify results match
        baseline_kstar = results['Baseline']['result'].log10_value
        fast_kstar = results['Fast']['result'].log10_value
        diff = abs(baseline_kstar - fast_kstar)
        print(f"Result difference: {diff:.6f} (should be < 0.001)")


def compare_epsilons(protein_file, ligand_file, complex_file, epsilons):
    """Compare computation time and convergence for different epsilon values."""
    print(f"\n=== Epsilon Comparison ===")
    
    protein = kstar_cpp.load_energy_matrix(protein_file)
    ligand = kstar_cpp.load_energy_matrix(ligand_file)
    complex_emat = kstar_cpp.load_energy_matrix(complex_file)
    
    workflow = kstar_cpp.KStarWorkflow()
    
    print(f"{'Epsilon':<10} {'Time (s)':<12} {'Confs (total)':<15} {'Converged':<10} {'log10(K*)':<15}")
    print("-" * 70)
    
    for epsilon in epsilons:
        start = time.perf_counter()
        result = workflow.compute(protein, ligand, complex_emat, epsilon=epsilon)
        elapsed = time.perf_counter() - start
        
        total_confs = (result.pfuncs.protein.num_confs + 
                      result.pfuncs.ligand.num_confs + 
                      result.pfuncs.complex.num_confs)
        
        print(f"{epsilon:<10.3f} {elapsed:<12.3f} {total_confs:<15,} {str(result.converged):<10} {result.log10_value:<15.6f}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description='Benchmark C++ Python bindings for K* computation'
    )
    parser.add_argument('protein_emat', help='Path to protein.emat.bin')
    parser.add_argument('ligand_emat', help='Path to ligand.emat.bin')
    parser.add_argument('complex_emat', help='Path to complex.emat.bin')
    parser.add_argument('--epsilon', type=float, default=0.99,
                       help='Epsilon for partition function (default: 0.99)')
    parser.add_argument('--runs', type=int, default=1,
                       help='Number of runs for averaging (default: 1)')
    parser.add_argument('--compare-variants', action='store_true',
                       help='Compare Baseline vs Fast A* variants')
    parser.add_argument('--compare-epsilons', action='store_true',
                       help='Compare different epsilon values')
    parser.add_argument('--epsilon-values', type=str, default="",
                       help='Comma-separated epsilon values for --compare-epsilons (e.g. "0.001,0.005,0.01,0.02,0.05"). '
                            'If omitted, defaults to "0.90,0.95,0.99,0.999".')
    
    args = parser.parse_args()
    
    print("="*70)
    print("K* Python Bindings Benchmark")
    print("="*70)
    print(f"\nInput files:")
    print(f"  Protein: {args.protein_emat}")
    print(f"  Ligand: {args.ligand_emat}")
    print(f"  Complex: {args.complex_emat}")
    print(f"  Epsilon: {args.epsilon}")
    
    # Main benchmark
    cpp_result = benchmark_cpp(args.protein_emat, args.ligand_emat, args.complex_emat, 
                               args.epsilon, num_runs=args.runs)
    
    # Optional comparisons
    if args.compare_variants:
        benchmark_cpp_variants(args.protein_emat, args.ligand_emat, args.complex_emat, args.epsilon)
    
    if args.compare_epsilons:
        if args.epsilon_values.strip():
            epsilons = [float(x) for x in args.epsilon_values.split(",") if x.strip()]
        else:
            epsilons = [0.90, 0.95, 0.99, 0.999]
        compare_epsilons(args.protein_emat, args.ligand_emat, args.complex_emat, epsilons)
    
    print("\n" + "="*70)
