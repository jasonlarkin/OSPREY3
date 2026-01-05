#!/usr/bin/env python3
"""
Minimal test script for kstar_cpp Python bindings.

This script tests that the module can be imported and basic functionality works.
"""

import sys
import os

# Add build directory to path if needed
build_dir = os.path.join(os.path.dirname(__file__), "../../../../build/cpp/kstar-python")
if os.path.exists(build_dir):
    sys.path.insert(0, build_dir)

try:
    import kstar_cpp
    print("Successfully imported kstar_cpp")
    print(f"  Module doc: {kstar_cpp.__doc__}")
except ImportError as e:
    print(f"Failed to import kstar_cpp: {e}")
    print(f"\nMake sure the module is built and in your PYTHONPATH:")
    print(f"  export PYTHONPATH=\"{build_dir}:$PYTHONPATH\"")
    sys.exit(1)

# Test basic API availability
print("\nTesting API availability...")
try:
    # Check classes exist
    assert hasattr(kstar_cpp, 'KStarWorkflow'), "KStarWorkflow not found"
    assert hasattr(kstar_cpp, 'EnergyMatrix'), "EnergyMatrix not found"
    assert hasattr(kstar_cpp, 'PartitionFunction'), "PartitionFunction not found"
    assert hasattr(kstar_cpp, 'load_energy_matrix'), "load_energy_matrix not found"
    print("  PASS: All expected classes and functions found")
except AssertionError as e:
    print(f"  FAIL: {e}")
    sys.exit(1)

# Test creating objects
print("\nTesting object creation...")
try:
    workflow = kstar_cpp.KStarWorkflow()
    pfunc = kstar_cpp.PartitionFunction()
    options = kstar_cpp.PartitionFunctionOptions()
    print("  PASS: Objects created successfully")
except Exception as e:
    print(f"  FAIL: Failed to create objects: {e}")
    sys.exit(1)

# Test enums
print("\nTesting enums...")
try:
    assert kstar_cpp.PartitionFunctionMethod.AStar is not None
    assert kstar_cpp.PartitionFunctionMethod.GradientDescent is not None
    assert kstar_cpp.AStarVariant.Baseline is not None
    assert kstar_cpp.AStarVariant.Fast is not None
    print("  PASS: Enums accessible")
except Exception as e:
    print(f"  FAIL: Enum test failed: {e}")
    sys.exit(1)

# Test loading energy matrix (will fail if file doesn't exist, but that's OK)
print("\nTesting energy matrix loading...")
test_file = "test.emat.bin"
if os.path.exists(test_file):
    try:
        emat = kstar_cpp.load_energy_matrix(test_file)
        print(f"  PASS: Loaded {test_file}")
        print(f"    Positions: {emat.get_num_positions()}")
    except Exception as e:
        print(f"  FAIL: Failed to load {test_file}: {e}")
else:
    print(f"  SKIP: {test_file} not found (this is OK for basic import test)")

print("\n" + "="*60)
print("Basic import and API tests passed!")
print("="*60)
print("\nTo test with actual data, use python_bindings_example.py")
print("or provide .emat.bin files exported from Java OSPREY.")
