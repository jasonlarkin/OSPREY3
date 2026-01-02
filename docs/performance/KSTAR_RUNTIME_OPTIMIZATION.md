# K* Runtime Optimization Strategies

## Current Issue

**Problem**: K* calculation takes hours to days for large systems (16,734 pairs)
**Crash**: Malloc assertion failure indicates native memory pressure

## Runtime Reduction Strategies

### 1. Relaxed Epsilon (Primary)

**Current**: epsilon=0.99 (very tight convergence)
**Fast**: epsilon=0.90-0.95 (faster convergence, still useful)

**Impact**:
- epsilon=0.99: 1-2 days (observed)
- epsilon=0.95: Estimated 4-8 hours (4-6x faster)
- epsilon=0.90: Estimated 1-2 hours (10-20x faster)

**Trade-off**: Lower accuracy, but sufficient for workload profiling

**Usage**:
```bash
./scripts/profile_kstar_fast_epsilon.sh 0.90
```

### 2. Increase Memory Limits

**Current crash**: Malloc assertion = native memory exhaustion

**Solutions**:
- Increase heap: `--heap-mib 8192` (vs. 4096)
- Increase direct memory: `--direct-mib 4096` (vs. 2048)
- Reduce CPU cores: `--cpu-cores 2` (reduces parallel memory pressure)

**Usage**:
```bash
python3 run_kstar_python.py \
    --epsilon 0.90 \
    --heap-mib 8192 \
    --direct-mib 4096 \
    --cpu-cores 2
```

### 3. Use Smaller Test Cases

**Strategy**: Profile smaller components first

**Options**:
- Protein-only: ~1,008 pairs (minutes)
- Ligand-only: ~7,575 pairs (10-30 minutes)
- Complex: ~16,734 pairs (hours to days)

**Approach**: Build scaling relationships from smaller systems, extrapolate to large

### 4. Limit Sequences

**Current**: Processes all sequences in space
**Fast**: Process single sequence or small subset

**Modification needed**: Add `--max-sequences N` flag to limit sequence count

### 5. Timeout/Checkpoint

**Strategy**: Run with timeout, checkpoint progress

**Implementation**: Add timeout wrapper, save intermediate results

## Recommended Approach

### For Workload Profiling

1. **Start with protein-only** (fastest, ~1K pairs)
   - Expected: Minutes
   - Establishes baseline

2. **Then ligand-only** (medium, ~7.5K pairs)
   - Expected: 10-30 minutes
   - Shows scaling

3. **Finally complex** (large, ~17K pairs)
   - Use epsilon=0.90
   - Increase memory: heap=8192MB, direct=4096MB
   - Reduce cores: 2 (less memory pressure)
   - Expected: 1-2 hours (vs. 1-2 days)

### Memory-Safe Configuration

```bash
# Conservative settings to avoid crashes
python3 run_kstar_python.py \
    --epsilon 0.90 \
    --heap-mib 8192 \
    --direct-mib 4096 \
    --cpu-cores 2 \
    --stop-on-oom
```

### Fast Profiling Configuration

```bash
# Maximum speed, relaxed accuracy
python3 run_kstar_python.py \
    --epsilon 0.85 \
    --heap-mib 4096 \
    --direct-mib 2048 \
    --cpu-cores 1
```

## Expected Runtime Comparison

| Configuration | Epsilon | Memory | Cores | Expected Time |
|---------------|---------|--------|-------|---------------|
| **Current** | 0.99 | 4GB/2GB | 4 | 1-2 days |
| **Fast** | 0.90 | 4GB/2GB | 4 | 1-2 hours |
| **Faster** | 0.90 | 8GB/4GB | 2 | 1-2 hours (more stable) |
| **Fastest** | 0.85 | 4GB/2GB | 1 | 30-60 minutes |


