# K* Crash Analysis: Malloc Assertion Failure

## Crash Details

**Error**: `python3: malloc.c:4302: _int_malloc: Assertion (unsigned long) (size) >= (unsigned long) (nb)' failed.`

**Context**:
- Epsilon: 0.90 (relaxed, should be faster)
- Memory: heap=4096MB, direct=2048MB
- Cores: 4
- Status: Energy matrices loaded from cache, crashed during K* calculation

## Root Cause

**Native memory exhaustion**: The malloc assertion indicates the system's native memory allocator failed, not JVM heap. This happens when:

1. **JNA direct buffers** exceed available native memory
2. **C++ energy calculations** allocate large native arrays
3. **System memory fragmentation** prevents large allocations
4. **WSL memory limits** restrict available native memory

## Solutions

### 1. Increase Direct Memory (Primary Fix)

**Current**: `--direct-mib 2048` (2GB)
**Recommended**: `--direct-mib 4096` or `--direct-mib 8192` (4-8GB)

Direct memory backs JNA buffers used by C++ energy calculations. If this is exhausted, malloc fails even if heap has space.

### 2. Reduce CPU Cores

**Current**: `--cpu-cores 4`
**Recommended**: `--cpu-cores 2` or `--cpu-cores 1`

Fewer cores = less parallel memory pressure. Each core may allocate native memory for energy calculations.

### 3. Increase Heap (Supporting)

**Current**: `--heap-mib 4096` (4GB)
**Recommended**: `--heap-mib 8192` (8GB)

More heap provides headroom, though the crash is native memory, not heap.

### 4. Check System Memory

**WSL memory limits**: WSL2 may have memory limits. Check:
```bash
free -h
cat /proc/meminfo | grep MemAvailable
```

If system memory is low, close other applications or increase WSL memory limit.

## Recommended Configuration

### Safe Configuration (Avoid Crashes)

```bash
./scripts/profile_kstar_fast_epsilon.sh 0.90 8192 4096 2
```

**Parameters**:
- Epsilon: 0.90
- Heap: 8192MB
- Direct: 4096MB
- Cores: 2

**Expected**: More stable, slightly slower due to fewer cores

### Maximum Speed (If System Has Memory)

```bash
./scripts/profile_kstar_fast_epsilon.sh 0.90 8192 8192 4
```

**Parameters**:
- Epsilon: 0.90
- Heap: 8192MB
- Direct: 8192MB (high for native buffers)
- Cores: 4

**Expected**: Fastest, but may still crash if system memory is limited

### Conservative (Most Stable)

```bash
./scripts/profile_kstar_fast_epsilon.sh 0.90 8192 4096 1
```

**Parameters**:
- Epsilon: 0.90
- Heap: 8192MB
- Direct: 4096MB
- Cores: 1 (sequential, minimal memory pressure)

**Expected**: Slowest but most stable

## Alternative: Use Smaller Test Case

If crashes persist, profile smaller systems first:

1. **Protein-only** (~1K pairs): Should complete in minutes
2. **Ligand-only** (~7.5K pairs): Should complete in 10-30 minutes
3. **Complex** (~17K pairs): Run last, with maximum memory

## Next Steps

1. **Check system memory**: `free -h` to see available memory
2. **Restart with safe config**: `./scripts/profile_kstar_fast_epsilon.sh 0.90 8192 4096 2`
3. **Monitor memory usage**: Watch `htop` or `top` during run
4. **If still crashes**: Use smaller test case or reduce to 1 core

