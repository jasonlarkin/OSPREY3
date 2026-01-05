#!/usr/bin/env python3
"""
Example usage of kstar_cpp Python bindings.

This demonstrates how to use the C++ K* implementation from Python.
"""

import kstar_cpp

# Load energy matrices (exported from Java as .emat.bin files)
print("Loading energy matrices...")
protein_emat = kstar_cpp.load_energy_matrix("protein.emat.bin")
ligand_emat = kstar_cpp.load_energy_matrix("ligand.emat.bin")
complex_emat = kstar_cpp.load_energy_matrix("complex.emat.bin")

print(f"Protein: {protein_emat.get_num_positions()} positions")
print(f"Ligand: {ligand_emat.get_num_positions()} positions")
print(f"Complex: {complex_emat.get_num_positions()} positions")

# Compute K* score using workflow (recommended)
print("\nComputing K* score...")
workflow = kstar_cpp.KStarWorkflow()
result = workflow.compute(
    protein=protein_emat,
    ligand=ligand_emat,
    complex=complex_emat,
    epsilon=0.99
)

print(f"\nK* Results:")
print(f"  log10(K*) = {result.log10_value:.6f}")
print(f"  Bounds: [{result.log10_lower_bound:.6f}, {result.log10_upper_bound:.6f}]")
print(f"  Converged: {result.converged}")

# Access individual partition function results
print(f"\nPartition Function Results:")
print(f"  Protein: log10(Q) = [{result.pfuncs.protein.lower_bound:.6f}, {result.pfuncs.protein.upper_bound:.6f}], "
      f"confs={result.pfuncs.protein.num_confs}, converged={result.pfuncs.protein.converged}")
print(f"  Ligand: log10(Q) = [{result.pfuncs.ligand.lower_bound:.6f}, {result.pfuncs.ligand.upper_bound:.6f}], "
      f"confs={result.pfuncs.ligand.num_confs}, converged={result.pfuncs.ligand.converged}")
print(f"  Complex: log10(Q) = [{result.pfuncs.complex.lower_bound:.6f}, {result.pfuncs.complex.upper_bound:.6f}], "
      f"confs={result.pfuncs.complex.num_confs}, converged={result.pfuncs.complex.converged}")

# Alternative: Compute partition functions individually (advanced use)
print("\n\nAlternative: Computing partition functions individually...")
pfunc = kstar_cpp.PartitionFunction()
protein_pfunc = pfunc.compute(protein_emat, epsilon=0.99)
print(f"Protein partition function: log10(Q) = [{protein_pfunc.lower_bound:.6f}, {protein_pfunc.upper_bound:.6f}]")

# Using different methods and options
print("\n\nUsing Fast A* variant...")
options = kstar_cpp.PartitionFunctionOptions()
options.astar_variant = kstar_cpp.AStarVariant.Fast
options.allow_exact_enumeration = False

result_fast = workflow.compute(
    protein=protein_emat,
    ligand=ligand_emat,
    complex=complex_emat,
    epsilon=0.99,
    method=kstar_cpp.PartitionFunctionMethod.AStar,
    options=options
)
print(f"Fast A* result: log10(K*) = {result_fast.log10_value:.6f}")
