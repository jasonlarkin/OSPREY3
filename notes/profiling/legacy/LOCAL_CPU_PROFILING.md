# Local CPU Profiling Tools for WSL

## Available Tools on WSL (CPU Only)

### C++ Profiling

#### 1. perf (Linux Performance Profiler)

**Installation on WSL:**

WSL2 uses Microsoft kernel which doesn't have matching perf packages in Ubuntu repositories:

```bash
# Try WSL-specific packages (may not be available)
sudo apt-get install linux-tools-standard-WSL2 linux-cloud-tools-standard-WSL2

# If not available, try generic package
sudo apt-get install linux-tools-generic
sudo ln -s /usr/lib/linux-tools/*/perf /usr/local/bin/perf
```

**WSL2 perf status:**
- Perf binary may install but show "WARNING: perf not found for kernel" when run
- WSL2 kernel doesn't expose perf events to userspace
- **Recommendation: Use gprof or Valgrind instead on WSL**

**If perf shows kernel warning:**
- Perf will not work for profiling on WSL2
- Use gprof (requires recompilation) or Valgrind callgrind (works without recompilation)

**Usage:**
```bash
# CPU profiling
perf record -g ./build/tests/confecalc_tests
perf report

# With call graphs
perf record -g --call-graph dwarf ./build/tests/confecalc_tests
perf report -g 'graph,0.5,caller'

# Specific events
perf record -e cpu-cycles,cache-misses ./build/tests/confecalc_tests
perf report
```

**Features:**
- CPU profiling
- Cache profiling
- Branch prediction profiling
- System calls profiling
- Hardware counters

**OSPREY Integration:**
```bash
cd src/main/cc/ConfEcalc
cmake -B build
cmake --build build
perf record -g ./build/tests/confecalc_tests
perf report
```

---

#### 2. gprof (GNU Profiler)

**Installation:**
```bash
# Usually pre-installed with gcc
gcc --version  # Verify gcc is installed
```

**Usage:**
```bash
# Compile with -pg flag
cd src/main/cc/ConfEcalc
cmake -B build -DCMAKE_CXX_FLAGS="-pg -O2"
cmake --build build

# Run and generate gmon.out
./build/tests/confecalc_tests

# Analyze
gprof ./build/tests/confecalc_tests gmon.out > profile.txt
```

**Features:**
- Function call counts
- Time spent per function
- Call graph
- Flat profile

**Limitations:**
- Only works with gcc
- Requires recompilation
- Higher overhead than sampling

---

#### 3. Valgrind (Callgrind)

**Installation:**
```bash
sudo apt-get update
sudo apt-get install valgrind
```

**Verify installation:**
```bash
valgrind --version
```

**Usage:**
```bash
# Call graph profiling
valgrind --tool=callgrind ./build/tests/confecalc_tests

# Analyze with callgrind_annotate
callgrind_annotate callgrind.out.*

# Or use KCacheGrind (GUI)
sudo apt-get install kcachegrind
kcachegrind callgrind.out.*
```

**Features:**
- Function call counts
- Call graph
- Cache simulation
- Branch prediction

**Limitations:**
- Very high overhead (10-50x slowdown)
- Not suitable for production profiling

---

#### 4. AddressSanitizer (ASan)

**Installation:**
```bash
# Part of GCC/Clang, no separate installation needed
```

**Usage:**
```bash
# Compile with ASan
cd src/main/cc/ConfEcalc
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address -g -O1"
cmake --build build

# Run (automatically detects issues)
./build/tests/confecalc_tests
```

**Detects:**
- Use-after-free
- Heap buffer overflows
- Stack buffer overflows
- Memory leaks

---

#### 5. ThreadSanitizer (TSan)

**Installation:**
```bash
# Part of GCC/Clang
```

**Usage:**
```bash
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1"
cmake --build build
./build/tests/confecalc_tests
```

**Detects:**
- Data races
- Deadlocks
- Thread safety issues

---

### Java Profiling

#### 1. Java Flight Recorder (JFR) - Built-in

**Usage:**
```bash
# Enable JFR at startup
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8" \
  -PjvmArgs="-XX:+UnlockDiagnosticVMOptions -XX:+FlightRecorder -XX:StartFlightRecording=duration=300s,filename=test.jfr"

# Or for existing process
jcmd <pid> JFR.start duration=60s filename=osprey.jfr
jcmd <pid> JFR.dump filename=osprey.jfr
jcmd <pid> JFR.stop
```

**Analysis:**
```bash
# Use JDK Mission Control (jmc) - requires GUI
# Or use jfr command-line tool
jfr print test.jfr
jfr summary test.jfr
```

**Features:**
- Very low overhead (< 1%)
- CPU profiling
- Memory profiling
- GC events
- Thread events
- I/O events

---

#### 2. async-profiler (Low Overhead)

**Installation:**
```bash
cd ~
wget https://github.com/async-profiler/async-profiler/releases/download/v2.9/async-profiler-2.9-linux-x64.tar.gz
tar -xzf async-profiler-2.9-linux-x64.tar.gz
cd async-profiler-2.9-linux-x64
```

**Usage:**
```bash
# CPU profiling (sampling)
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8" &
PID=$!
java -jar async-profiler.jar -e cpu -d 300 -f cpu.html $PID
wait $PID

# Allocation profiling
java -jar async-profiler.jar -e alloc -d 300 -f alloc.html $PID

# Wall-clock profiling
java -jar async-profiler.jar -e wall -d 300 -f wall.html $PID
```

**Features:**
- Very low overhead (< 2%)
- CPU profiling (user, kernel, both)
- Allocation profiling
- Lock profiling
- Flame graph generation

---

#### 3. VisualVM (Free, Built-in)

**Installation:**
```bash
# Included with JDK (jvisualvm command)
# Or download standalone
wget https://github.com/oracle/visualvm/releases/download/2.1.7/visualvm_217.zip
unzip visualvm_217.zip
```

**Usage:**
```bash
# Start VisualVM
jvisualvm

# Or attach to running process
jvisualvm --jdkhome /path/to/jdk
```

**Features:**
- CPU profiling (sampling, instrumentation)
- Memory profiling (heap dump, GC monitoring)
- Thread monitoring
- MBeans browser
- Heap walker

**OSPREY Integration:**
```bash
# Run OSPREY with JMX
java -Dcom.sun.management.jmxremote \
     -Dcom.sun.management.jmxremote.port=9999 \
     -Dcom.sun.management.jmxremote.authenticate=false \
     -Dcom.sun.management.jmxremote.ssl=false \
     -jar osprey.jar

# Connect VisualVM to localhost:9999
```

---

#### 4. JProfiler (Commercial, Free for Open Source)

**Installation:**
```bash
# Download from https://www.ej-technologies.com/products/jprofiler/overview.html
# Extract and run
./jprofiler.sh
```

**Usage:**
```bash
# Attach to running JVM
jprofiler -attach <pid>

# Or launch with JProfiler
jprofiler -jar osprey.jar
```

**Features:**
- CPU profiling (sampling, instrumentation)
- Memory profiling (heap, allocations, GC)
- Thread profiling (deadlocks, contention)
- I/O profiling (file, network)
- Database profiling (JDBC)

---

### Quick Setup Script

**Create `setup-profiling-tools.sh`:**
```bash
#!/bin/bash
# Setup profiling tools for WSL

echo "Installing perf..."
sudo apt-get update
sudo apt-get install -y linux-tools-standard-WSL2 linux-cloud-tools-standard-WSL2 || \
sudo apt-get install -y linux-tools-generic
sudo ln -sf /usr/lib/linux-tools/*/perf /usr/local/bin/perf 2>/dev/null || echo "perf symlink created or already exists"

echo "Installing Valgrind..."
sudo apt-get install -y valgrind
# Optional: kcachegrind for GUI (requires X11)
# sudo apt-get install -y kcachegrind

echo "Installing async-profiler..."
cd ~
if [ ! -d "async-profiler-2.9-linux-x64" ]; then
    wget https://github.com/async-profiler/async-profiler/releases/download/v2.9/async-profiler-2.9-linux-x64.tar.gz
    tar -xzf async-profiler-2.9-linux-x64.tar.gz
fi

echo "Verifying Java tools..."
java -version
jfr --version 2>/dev/null || echo "JFR available via jcmd"

echo "Setup complete."
```

**Run:**
```bash
chmod +x setup-profiling-tools.sh
./setup-profiling-tools.sh
```

---

## Recommended Workflow for OSPREY

### Step 1: Baseline Measurement

```bash
# Run without profiling to get baseline
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8"
# Note execution time
```

### Step 2: Java CPU Profiling

```bash
# Profile with low overhead (JFR)
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8" \
  -PjvmArgs="-XX:+UnlockDiagnosticVMOptions -XX:+FlightRecorder -XX:StartFlightRecording=duration=300s,filename=test.jfr"

# Or with async-profiler
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8" &
PID=$!
java -jar ~/async-profiler-2.9-linux-x64/async-profiler.jar -e cpu -d 300 -f cpu.html $PID
wait $PID
```

### Step 3: C++ CPU Profiling

**Option A: gprof (Recommended for WSL)**
```bash
cd src/main/cc/ConfEcalc
cmake -B build -DCMAKE_CXX_FLAGS="-pg -O2"
cmake --build build
./build/tests/confecalc_tests
gprof ./build/tests/confecalc_tests gmon.out > profile.txt
```

**Option B: Valgrind callgrind (Alternative for WSL)**
```bash
cd src/main/cc/ConfEcalc
cmake -B build
cmake --build build
valgrind --tool=callgrind ./build/tests/confecalc_tests
callgrind_annotate callgrind.out.*
```

### Step 4: Memory Profiling (if needed)

```bash
# Java allocation profiling
./gradlew test --tests "edu.duke.cs.osprey.gmec.TestFindGMEC.test1CC8" &
PID=$!
java -jar ~/async-profiler-2.9-linux-x64/async-profiler.jar -e alloc -d 300 -f alloc.html $PID
wait $PID

# C++ memory leak detection
cd src/main/cc/ConfEcalc
cmake -B build -DCMAKE_CXX_FLAGS="-fsanitize=address -g -O1"
cmake --build build
./build/tests/confecalc_tests
```

---

## Tool Comparison

| Tool | Overhead | Ease of Use | Best For | WSL Status |
|------|----------|-------------|----------|------------|
| **perf** | Low (< 5%) | Medium | C++ CPU profiling, system-wide | Not available on WSL2 |
| **gprof** | Medium (10-20%) | Easy | C++ function-level profiling | Works on WSL |
| **Valgrind** | Very High (10-50x) | Easy | Detailed call graphs, cache analysis | Works on WSL |
| **JFR** | Very Low (< 1%) | Easy | Java production profiling |
| **async-profiler** | Low (< 2%) | Easy | Java CPU/allocation profiling |
| **VisualVM** | Low-Medium | Easy | Java GUI-based profiling |
| **AddressSanitizer** | Medium (2-3x) | Easy | C++ memory error detection |

---

## WSL-Specific Notes

**perf on WSL:**
- WSL2 uses Microsoft kernel (`*-microsoft-standard-WSL2`)
- WSL-specific packages (`linux-tools-standard-WSL2`) are not available in Ubuntu repos
- Generic package (`linux-tools-generic`) may install but perf shows "WARNING: perf not found for kernel"
- **Perf does not work on WSL2** - kernel doesn't expose perf events to userspace
- **Use gprof or Valgrind instead** - both work reliably on WSL

**Java Tools:**
- All Java profiling tools work normally on WSL
- JFR requires JDK 11+ (check with `java -version`)

**GUI Tools:**
- VisualVM requires X11 forwarding or Windows X server (Xming, VcXsrv)
- KCacheGrind requires X11 for GUI
- Command-line tools (perf, gprof, async-profiler) work without GUI

**File System:**
- WSL file system performance differs from native Linux
- For best performance, run tests in WSL file system (`/home/...`) not Windows mount (`/mnt/c/...`)

---

## Quick Test Commands

**Test perf:**
```bash
# Check if perf is available
perf --version

# If shows "WARNING: perf not found for kernel", perf will not work on WSL2
# Use gprof or Valgrind instead
```

**WSL Recommendation:**
- Skip perf on WSL2 - it doesn't work
- Use **gprof** for C++ profiling (requires recompilation with `-pg`)
- Use **Valgrind callgrind** for detailed analysis (works without recompilation)

**Test async-profiler:**
```bash
java -jar ~/async-profiler-2.9-linux-x64/async-profiler.jar --version
```

**Test JFR:**
```bash
java -XX:+UnlockDiagnosticVMOptions -XX:+FlightRecorder -version
```

**Test Valgrind:**
```bash
valgrind --version
```

---

## Next Steps (GPU on Google Colab)

When ready for GPU profiling:
- CUDA profiling: `nvprof`, `nsight compute`
- CUDA memory: `compute-sanitizer`
- CUDA timeline: `nsight systems`

