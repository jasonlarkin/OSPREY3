# Local Development Setup

## Overview

This guide sets up a local development environment for implementing the C++20/23 parallel K* implementation.

## Prerequisites

### Required Tools

1. **GCC 11+** (C++20 support)
   ```bash
   # Ubuntu/Debian
   sudo apt-get install gcc-11 g++-11
   
   # Verify
   gcc-11 --version
   g++-11 --version
   ```

2. **CMake 3.20+**
   ```bash
   sudo apt-get install cmake
   cmake --version
   ```

3. **Build tools**
   ```bash
   sudo apt-get install build-essential
   ```

### Optional (for profiling)

4. **Valgrind** (memory profiling)
   ```bash
   sudo apt-get install valgrind
   ```

5. **perf** (CPU profiling)
   ```bash
   sudo apt-get install linux-perf
   ```

6. **MPI** (for future multi-node)
   ```bash
   sudo apt-get install libopenmpi-dev openmpi-bin
   ```

## Project Structure

```
osprey-fork_modern/
├── src/
│   ├── main/
│   │   ├── cc/                    # Existing C++ code
│   │   │   ├── ConfEcalc/
│   │   │   └── IntelConfEcalc/
│   │   └── cpp/                   # NEW: C++20/23 K* implementation
│   │       ├── kstar/
│   │       │   ├── kstar_parallel.hpp
│   │       │   ├── kstar_parallel.cpp
│   │       │   ├── partition_function.hpp
│   │       │   ├── partition_function.cpp
│   │       │   ├── arena.hpp
│   │       │   ├── arena.cpp
│   │       │   ├── thread_pool.hpp
│   │       │   └── thread_pool.cpp
│   │       └── CMakeLists.txt
│   └── test/
│       └── cpp/
│           └── kstar/
│               └── test_kstar_parallel.cpp
├── build/
│   └── cpp/                       # Build directory
└── docs/
    └── performance/
        └── CXX_IMPLEMENTATION_SPEC.md
```

## Initial Setup

### 1. Create Directory Structure

```bash
cd osprey-fork_modern
mkdir -p src/main/cpp/kstar
mkdir -p src/test/cpp/kstar
mkdir -p build/cpp
```

### 2. Create CMakeLists.txt

**`src/main/cpp/kstar/CMakeLists.txt`**:
```cmake
cmake_minimum_required(VERSION 3.20)
project(osprey_kstar_parallel)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O3 -march=native")

# Source files
set(SOURCES
    kstar_parallel.cpp
    partition_function.cpp
    arena.cpp
    thread_pool.cpp
)

# Headers
set(HEADERS
    kstar_parallel.hpp
    partition_function.hpp
    arena.hpp
    thread_pool.hpp
)

# Create library
add_library(osprey_kstar_parallel STATIC ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(osprey_kstar_parallel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src/main/cc/ConfEcalc
)

# Link libraries
target_link_libraries(osprey_kstar_parallel
    pthread
)

# Executable (for testing)
add_executable(kstar_test test_kstar_parallel.cpp)
target_link_libraries(kstar_test osprey_kstar_parallel)
```

### 3. Build Configuration

**`build/cpp/CMakeLists.txt`** (or configure from root):
```bash
cd build/cpp
cmake ../../src/main/cpp/kstar \
    -DCMAKE_C_COMPILER=gcc-11 \
    -DCMAKE_CXX_COMPILER=g++-11 \
    -DCMAKE_BUILD_TYPE=Release
```

### 4. Build

```bash
cd build/cpp
make -j$(nproc)
```

## Development Workflow

### 1. Start with Minimal Implementation

**Phase 1.1: Basic Structure**
- Create header files with class declarations
- Implement empty methods
- Add basic unit tests

**Phase 1.2: ConfSpace Integration**
- Integrate with existing C++ ConfSpace
- Test ConfSpace loading
- Validate data access

**Phase 1.3: PartitionFunction**
- Implement basic A* search
- Test on small examples
- Compare with Java results

**Phase 1.4: Parallel Processing**
- Implement ThreadPool
- Add parallel sequence processing
- Validate speedup

### 2. Testing Strategy

**Unit Tests**:
```cpp
// test_kstar_parallel.cpp
#include <cassert>
#include "kstar_parallel.hpp"

void test_arena() {
    osprey::Arena<int> arena(1024);
    int* ptr = arena.allocate<int>(10);
    assert(ptr != nullptr);
    arena.reset();
}

void test_sequence() {
    osprey::Sequence seq;
    seq.residue_assignments = {0, 1, 2};
    assert(seq.residue_assignments.size() == 3);
}
```

**Integration Tests**:
- Load real ConfSpace from `.ccsx` files
- Compare K* scores with Java implementation
- Validate on known test cases

### 3. Profiling

**Memory Profiling**:
```bash
valgrind --leak-check=full ./kstar_test
```

**CPU Profiling**:
```bash
perf record ./kstar_test
perf report
```

**Time Profiling**:
```cpp
#include <chrono>

auto start = std::chrono::high_resolution_clock::now();
// ... code ...
auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
std::cout << "Time: " << duration.count() << " ms\n";
```

## Integration with Existing OSPREY

### Phase 1: Standalone C++ Implementation

- Independent build system
- Test with existing `.ccsx` files
- Validate correctness

### Phase 2: Python Bindings (Future)

- Use pybind11 or ctypes
- Create Python API
- Integrate with existing Python workflow

### Phase 3: Replace Java K* (Future)

- Update Python workflow to call C++ implementation
- Maintain backward compatibility
- Gradual migration

## Development Tips

### 1. Start Small

- Begin with single-threaded implementation
- Add parallelism incrementally
- Test each component independently

### 2. Validate Early

- Compare results with Java implementation
- Use known test cases
- Add assertions for correctness

### 3. Profile Often

- Identify bottlenecks early
- Measure speedup at each stage
- Optimize based on data

### 4. Use Modern C++ Features

- `std::execution` for parallelism
- Concepts for type safety
- Ranges for functional style
- Smart pointers for memory management

## Next Steps

1. **Create directory structure**
2. **Set up CMake build system**
3. **Implement basic Arena allocator**
4. **Implement basic Sequence structure**
5. **Integrate with ConfSpace**
6. **Implement PartitionFunction skeleton**
7. **Add unit tests**
8. **Incremental development**

## Resources

- **C++20 Standard**: https://en.cppreference.com/w/cpp/20
- **CMake Documentation**: https://cmake.org/documentation/
- **Existing OSPREY C++ Code**: `src/main/cc/ConfEcalc/`
- **Implementation Spec**: `docs/performance/CXX_IMPLEMENTATION_SPEC.md`

