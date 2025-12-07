#!/bin/bash

# Build and test DeepCopy library

set -e

cd "$(dirname "$0")"

# Create build directory
mkdir -p build
cd build

# Configure CMake with tests enabled
cmake .. -DBUILD_TESTS=ON

# Build
cmake --build .

# Run tests
ctest --output-on-failure

echo "Build and tests completed successfully"

