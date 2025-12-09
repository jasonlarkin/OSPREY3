# Code Analysis Tools for OSPREY C++ Components

## Overview

This document provides tools and techniques for analyzing C++ code complexity, test coverage, and code quality in the OSPREY codebase.

---

## Static Analysis Tools

### 1. Complexity Metrics

#### cppcheck
**Purpose:** Static code analysis for C/C++  
**Installation:**
```bash
sudo apt-get install cppcheck  # Linux
brew install cppcheck          # macOS
```

**Usage:**
```bash
# Basic check
cppcheck --enable=all src/main/cc/ConfEcalc/

# With XML output for reporting
cppcheck --xml --xml-version=2 --enable=all src/main/cc/ConfEcalc/ 2> cppcheck-report.xml

# Check specific complexity
cppcheck --enable=all --check-level=exhaustive src/main/cc/
```

**Key Features:**
- Detects bugs, memory leaks, undefined behavior
- Complexity analysis
- Style checking

#### Clang Static Analyzer
**Purpose:** Static analysis using Clang  
**Installation:**
```bash
sudo apt-get install clang-tools  # Linux
# Already available with Clang
```

**Usage:**
```bash
# Scan build
scan-build cmake -B build -S src/main/cc/ConfEcalc
scan-build cmake --build build

# Or for existing build
scan-build make -C build
```

**Output:** HTML reports in `/tmp/scan-build-*/`

#### Complexity Metrics Tools

**Lizard** - Cyclomatic Complexity Analyzer
```bash
pip install lizard

# Analyze C++ files
lizard src/main/cc/ConfEcalc/ -l cpp

# Generate HTML report
lizard src/main/cc/ConfEcalc/ -l cpp --html > complexity-report.html

# Focus on high complexity
lizard src/main/cc/ -l cpp --threshold cyclomatic_complexity=10
```

**Metrics provided:**
- Cyclomatic complexity
- Function length (NLOC - Non-comment Lines of Code)
- Token count
- Parameter count
- Maximum nesting depth

**Example Output:**
```
================================================
  NLOC    CCN   token  PARAM  length  location  
------------------------------------------------
    156     15    450      4     156  confecalc.cc::osprey::minimize<float64_t>
    89      8    234      2      89   energy_ambereef1.h::calc
```

### 2. Code Statistics

#### cloc
**Purpose:** Count lines of code  
**Installation:**
```bash
sudo apt-get install cloc  # Linux
brew install cloc          # macOS
```

**Usage:**
```bash
# Count all C++ code
cloc src/main/cc/

# By component
cloc src/main/cc/ConfEcalc/
cloc src/main/cc/DeepCopy/

# Exclude test files
cloc src/main/cc/ --exclude-dir=tests

# Output to file
cloc src/main/cc/ --report-file=loc-report.txt
```

**Provides:**
- Lines of code (total, blank, comment, source)
- File counts
- Language breakdown

#### scc (Sloc, Cloc, and Code)
**Purpose:** Fast code counter  
**Installation:**
```bash
# Download from: https://github.com/boyter/scc/releases
# Or
go install github.com/boyter/scc/v3@latest
```

**Usage:**
```bash
scc src/main/cc/

# With complexity estimation
scc src/main/cc/ --no-cocomo
```

### 3. Static Analysis with Modern Tools

#### SonarQube / SonarLint
**Purpose:** Comprehensive code quality analysis  
**Installation:**
```bash
# SonarLint (IDE plugin) - Available for VS Code, IntelliJ, etc.
# SonarQube (Server) - Requires Docker or server setup
```

**Features:**
- Code smells detection
- Security vulnerabilities
- Code coverage integration
- Technical debt estimation
- Complexity metrics

#### PVS-Studio (Commercial)
**Purpose:** Deep static analysis  
**Note:** Has free license for open source projects

#### cpplint / cpplint-style
**Purpose:** Google C++ Style Guide checker  
**Installation:**
```bash
pip install cpplint
```

**Usage:**
```bash
cpplint --recursive src/main/cc/ConfEcalc/
```

---

## Dynamic Analysis Tools

### 1. Runtime Analysis

#### Valgrind
**Purpose:** Memory error detection  
**Installation:**
```bash
sudo apt-get install valgrind  # Linux
```

**Usage:**
```bash
# Memory leak detection
valgrind --leak-check=full --show-leak-kinds=all \
  --track-origins=yes ./build/tests/confecalc_tests

# Generate report
valgrind --leak-check=full --log-file=valgrind-report.txt \
  ./build/tests/confecalc_tests
```

**Tools:**
- `memcheck` - Memory errors
- `callgrind` - Call graph profiling
- `cachegrind` - Cache profiling
- `helgrind` - Thread error detection

#### AddressSanitizer (ASan)
**Purpose:** Fast memory error detector (LLVM/GCC)  
**Usage:**
```bash
# Compile with ASan
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address -g -O1"
cmake --build build

# Run tests
./build/tests/confecalc_tests
```

**Detects:**
- Use-after-free
- Heap buffer overflows
- Stack buffer overflows
- Memory leaks

#### ThreadSanitizer (TSan)
**Purpose:** Data race detection  
**Usage:**
```bash
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"
```

#### MemorySanitizer (MSan)
**Purpose:** Uninitialized memory detection  
**Usage:**
```bash
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=memory -g -O1"
```

### 2. Profiling

#### gprof
**Purpose:** Function-level profiling  
**Usage:**
```bash
# Compile with -pg flag
cmake -B build -DCMAKE_CXX_FLAGS="-pg -O2"
cmake --build build

# Run and generate gmon.out
./build/tests/confecalc_tests

# Analyze
gprof ./build/tests/confecalc_tests gmon.out > profile.txt
```

#### perf (Linux)
**Purpose:** System-wide performance analysis  
**Usage:**
```bash
# Record
perf record ./build/tests/confecalc_tests

# Report
perf report

# With call graphs
perf record -g ./build/tests/confecalc_tests
perf report -g 'graph,0.5,caller'
```

#### Intel VTune Profiler
**Purpose:** Advanced performance profiling (Commercial, free for open source)

---

## Test Coverage Analysis

### 1. Code Coverage Tools

#### gcov + lcov
**Purpose:** GCC code coverage  
**Installation:**
```bash
sudo apt-get install lcov  # Linux
```

**Usage:**
```bash
# Compile with coverage flags
cmake -B build \
  -DCMAKE_CXX_FLAGS="--coverage -g -O0" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage"

cmake --build build --target confecalc_tests

# Run tests
./build/tests/confecalc_tests

# Generate coverage
lcov --capture --directory build --output-file coverage.info
lcov --remove coverage.info '/usr/*' --output-file coverage.info
genhtml coverage.info --output-directory coverage-report
```

**Output:** HTML report in `coverage-report/`

#### llvm-cov (Clang)
**Purpose:** Clang code coverage  
**Usage:**
```bash
# Compile with coverage
cmake -B build \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping"

cmake --build build --target confecalc_tests

# Run tests
LLVM_PROFILE_FILE="profraw" ./build/tests/confecalc_tests

# Generate report
llvm-profdata merge -sparse profraw -o profdata
llvm-cov show ./build/tests/confecalc_tests -instr-profile=profdata
llvm-cov report ./build/tests/confecalc_tests -instr-profile=profdata
```

### 2. Coverage Integration with CMake

**Add to CMakeLists.txt:**
```cmake
if(CMAKE_BUILD_TYPE STREQUAL "Coverage")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} --coverage")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} --coverage")
endif()
```

**Usage:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Coverage
cmake --build build
```

### 3. Coverage Analysis for Specific Components

**Example script:**
```bash
#!/bin/bash
# analyze-coverage.sh

COMPONENT=$1  # e.g., ConfEcalc, DeepCopy

cd src/main/cc/${COMPONENT}

# Run tests with coverage
cmake -B build -DCMAKE_BUILD_TYPE=Coverage
cmake --build build
./build/tests/${COMPONENT,,}_tests

# Generate report
lcov --capture --directory build --output-file ${COMPONENT}-coverage.info
lcov --remove ${COMPONENT}-coverage.info '/usr/*' --output-file ${COMPONENT}-coverage.info
genhtml ${COMPONENT}-coverage.info --output-directory ${COMPONENT}-coverage-report

echo "Coverage report: ${COMPONENT}-coverage-report/index.html"
```

---

## Complexity Metrics Analysis

### 1. Cyclomatic Complexity

**Using Lizard:**
```bash
# Generate complexity report
lizard src/main/cc/ConfEcalc/ -l cpp --csv > complexity.csv

# Filter high complexity functions
lizard src/main/cc/ConfEcalc/ -l cpp | grep -E "^\s*[0-9]+\s+[1-9][0-9]+"
```

**Interpretation:**
- 1-10: Simple (green)
- 11-20: Moderate (yellow)
- 21-50: Complex (orange)
- 50+: Very complex (red) - consider refactoring

### 2. Function-Level Analysis

**Script to extract function complexity:**
```python
#!/usr/bin/env python3
# analyze-complexity.py

import subprocess
import csv
import sys

def analyze_complexity(directory):
    result = subprocess.run(
        ['lizard', directory, '-l', 'cpp', '--csv'],
        capture_output=True,
        text=True
    )
    
    reader = csv.DictReader(result.stdout.splitlines())
    complex_functions = []
    
    for row in reader:
        complexity = int(row['CCN'])
        if complexity >= 10:  # Threshold
            complex_functions.append({
                'file': row['file'],
                'function': row['function'],
                'complexity': complexity,
                'nloc': row['NLOC'],
                'params': row['PARAM']
            })
    
    # Sort by complexity
    complex_functions.sort(key=lambda x: x['complexity'], reverse=True)
    
    print("High Complexity Functions:")
    print(f"{'Function':<50} {'Complexity':<12} {'Lines':<8} {'File'}")
    print("-" * 100)
    for func in complex_functions[:20]:  # Top 20
        print(f"{func['function']:<50} {func['complexity']:<12} {func['nloc']:<8} {func['file']}")

if __name__ == '__main__':
    analyze_complexity(sys.argv[1] if len(sys.argv) > 1 else 'src/main/cc/')
```

---

## Dependency Analysis

### 1. Include Graph

**include-what-you-use**
**Purpose:** Analyze include dependencies  
**Installation:**
```bash
sudo apt-get install include-what-you-use  # Linux
```

**Usage:**
```bash
# Analyze includes
include-what-you-use -Xiwyu --mapping_file=iwyu.imp src/main/cc/ConfEcalc/confecalc.cc
```

### 2. Dependency Graph

**cinclude2dot**
**Purpose:** Generate include dependency graphs  
**Installation:**
```bash
pip install cinclude2dot
```

**Usage:**
```bash
# Generate dot file
cinclude2dot src/main/cc/ConfEcalc/ > includes.dot

# Convert to image
dot -Tpng includes.dot -o includes.png
```

---

## Code Quality Metrics

### 1. Maintainability Index

**Tools:**
- **SonarQube** provides maintainability ratings
- **Code Climate** (commercial)
- **Custom calculation:**
  ```
  MI = 171 - 5.2 * ln(Halstead Volume) - 0.23 * (Cyclomatic Complexity) 
       - 16.2 * ln(Lines of Code) + 50 * sin(sqrt(2.4 * Comment Density))
  ```

### 2. Technical Debt

**Estimate based on:**
- Code smells (SonarQube)
- Complexity metrics
- Test coverage gaps
- Duplication detection

---

## Automated Analysis Script

**Example comprehensive analysis script:**
```bash
#!/bin/bash
# comprehensive-analysis.sh

COMPONENT=${1:-"ConfEcalc"}
OUTPUT_DIR="analysis/${COMPONENT}"
mkdir -p ${OUTPUT_DIR}

cd src/main/cc/${COMPONENT}

echo "=== Lines of Code ==="
cloc . > ${OUTPUT_DIR}/loc.txt
cat ${OUTPUT_DIR}/loc.txt

echo -e "\n=== Complexity Analysis ==="
lizard . -l cpp --csv > ${OUTPUT_DIR}/complexity.csv
lizard . -l cpp --html > ${OUTPUT_DIR}/complexity.html

echo -e "\n=== Static Analysis ==="
cppcheck --enable=all --xml --xml-version=2 . 2> ${OUTPUT_DIR}/cppcheck.xml

echo -e "\n=== Code Coverage (if tests exist) ==="
if [ -d "tests" ]; then
    cmake -B build -DCMAKE_BUILD_TYPE=Coverage
    cmake --build build
    if [ -f "./build/tests/${COMPONENT,,}_tests" ]; then
        ./build/tests/${COMPONENT,,}_tests
        lcov --capture --directory build --output-file ${OUTPUT_DIR}/coverage.info
        lcov --remove ${OUTPUT_DIR}/coverage.info '/usr/*' --output-file ${OUTPUT_DIR}/coverage.info
        genhtml ${OUTPUT_DIR}/coverage.info --output-directory ${OUTPUT_DIR}/coverage-report
    fi
fi

echo -e "\nAnalysis complete. Results in: ${OUTPUT_DIR}/"
```

---

## CI/CD Integration

### GitHub Actions Example

```yaml
name: Code Analysis

on: [push, pull_request]

jobs:
  analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install dependencies
        run: |
          sudo apt-get update
          sudo apt-get install -y cppcheck cloc lizard lcov
          pip install lizard
      
      - name: Lines of Code
        run: cloc src/main/cc/ --report-file=loc-report.txt
      
      - name: Complexity Analysis
        run: lizard src/main/cc/ -l cpp --html > complexity-report.html
      
      - name: Static Analysis
        run: cppcheck --enable=all --xml --xml-version=2 src/main/cc/ 2> cppcheck-report.xml
      
      - name: Upload reports
        uses: actions/upload-artifact@v3
        with:
          name: analysis-reports
          path: |
            loc-report.txt
            complexity-report.html
            cppcheck-report.xml
```

---

## Recommended Workflow

1. **Initial Analysis:**
   ```bash
   # Get baseline metrics
   cloc src/main/cc/
   lizard src/main/cc/ -l cpp
   ```

2. **Static Analysis:**
   ```bash
   # Run static analyzers
   cppcheck --enable=all src/main/cc/
   scan-build cmake --build build
   ```

3. **Test Coverage:**
   ```bash
   # Measure coverage
   cmake -B build -DCMAKE_BUILD_TYPE=Coverage
   cmake --build build
   ./build/tests/*_tests
   lcov --capture --directory build --output-file coverage.info
   ```

4. **Identify Problem Areas:**
   ```bash
   # High complexity functions
   lizard src/main/cc/ -l cpp | grep -E "^\s*[0-9]+\s+[1-9][0-9]+"
   
   # Low coverage files
   genhtml coverage.info --output-directory coverage-report
   # Review coverage-report/index.html
   ```

5. **Prioritize:**
   - Functions with complexity > 20
   - Files with coverage < 80%
   - Code smells from static analysis
   - Edge cases in complex functions

---

## Tools Summary

| Tool | Purpose | Type | Installation |
|------|---------|------|--------------|
| **cppcheck** | Static analysis | Static | `apt-get install cppcheck` |
| **lizard** | Complexity metrics | Static | `pip install lizard` |
| **cloc** | Lines of code | Static | `apt-get install cloc` |
| **gcov/lcov** | Code coverage | Dynamic | `apt-get install lcov` |
| **valgrind** | Memory errors | Dynamic | `apt-get install valgrind` |
| **AddressSanitizer** | Memory errors | Dynamic | Built-in (GCC/Clang) |
| **perf** | Profiling | Dynamic | Built-in (Linux) |
| **SonarQube** | Comprehensive QA | Static/Dynamic | Docker/Server |

---

## Next Steps

1. Run initial analysis to establish baseline metrics
2. Set up CI/CD integration for automated analysis
3. Identify high-complexity functions for refactoring
4. Measure test coverage and identify gaps
5. Create coverage targets (e.g., 80% for critical paths)
6. Track metrics over time to measure improvement

