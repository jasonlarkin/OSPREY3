# Workload Variation Profiling Plan

## Objective

Characterize K* performance across system sizes to understand:
- Time scaling: pairs → computation time
- Memory scaling: pairs → memory usage
- GC overhead: system size → GC frequency/cost
- Bottleneck identification: which operations scale poorly

## Test Cases

### Small System (Baseline)
- **Pairs**: ~1,000 (protein only)
- **Sequences**: 1-5
- **Expected time**: Minutes
- **Use case**: Fast iteration, unit testing

### Medium System (Representative)
- **Pairs**: ~10,000 (ligand-like)
- **Sequences**: 10-20
- **Expected time**: Hours
- **Use case**: Production-like workload

### Large System (Stress Test)
- **Pairs**: ~17,000 (complex, like 2RL0)
- **Sequences**: 50-100
- **Expected time**: 1-2 days
- **Use case**: Maximum stress, performance validation

## Profiling Metrics

### Time Metrics
- Total execution time
- Per-sequence time
- Per-pair time (time / pairs)
- Energy calculation time
- A* search time
- Partition function time
- GC pause time

### Memory Metrics
- Peak memory usage
- Memory per sequence
- Memory per pair
- GC frequency
- GC pause duration
- Allocation rate

### Scaling Relationships
- Time = f(pairs, sequences)
- Memory = f(pairs, sequences)
- GC overhead = f(memory pressure)

## Profiling Tools

### Java/K* Profiling
- **JVM GC logs**: `-Xlog:gc*:file=gc.log`
- **JVM CPU profiling**: `-XX:+UnlockDiagnosticVMOptions -XX:+LogCompilation`
- **JVM allocation profiling**: `-XX:+HeapDumpOnOutOfMemoryError`
- **cProfile** (Python wrapper): Already implemented

### System Profiling
- **perf**: CPU hotspots, cache misses
- **valgrind**: Memory leaks, allocation patterns
- **htop/top**: Real-time resource usage

### Custom Instrumentation
- Add timing markers in K* code
- Log memory usage at key points
- Track allocation counts

## Execution Plan

### Step 1: Small System Profiling
```bash
# Run K* on small system
./run_kstar_python.py \
    --confspace small.ccsx \
    --sequences small_sequences.txt \
    --heap-mib 512 \
    --direct-mib 256 \
    -Xlog:gc*:file=gc_small.log

# Collect metrics
python3 scripts/analyze_kstar_profile.py \
    --gc-log gc_small.log \
    --output small_profile.json
```

### Step 2: Medium System Profiling
```bash
# Run K* on medium system
./run_kstar_python.py \
    --confspace medium.ccsx \
    --sequences medium_sequences.txt \
    --heap-mib 2048 \
    --direct-mib 1024 \
    -Xlog:gc*:file=gc_medium.log

# Collect metrics
python3 scripts/analyze_kstar_profile.py \
    --gc-log gc_medium.log \
    --output medium_profile.json
```

### Step 3: Large System Profiling
```bash
# Run K* on large system (2RL0-like)
./run_kstar_python.py \
    --confspace complex.ccsx \
    --sequences large_sequences.txt \
    --heap-mib 4096 \
    --direct-mib 2048 \
    -Xlog:gc*:file=gc_large.log

# Collect metrics
python3 scripts/analyze_kstar_profile.py \
    --gc-log gc_large.log \
    --output large_profile.json
```

### Step 4: Analysis
```bash
# Compare across system sizes
python3 scripts/compare_workloads.py \
    --profiles small_profile.json medium_profile.json large_profile.json \
    --output workload_scaling.md
```

## Expected Outputs

### Scaling Relationships
- Time vs. pairs (linear? quadratic? exponential?)
- Memory vs. pairs (linear? sublinear?)
- GC overhead vs. memory pressure
- Per-sequence overhead (fixed? variable?)

### Bottleneck Identification
- Which operations dominate at each scale?
- Where does performance degrade?
- What are the breakpoints?

### Optimization Targets
- Which operations benefit most from parallelization?
- What are the memory hot spots?

## Deliverables

1. **Workload scaling report**: `pipeline_analysis/kstar/WORKLOAD_SCALING.md`
   - Time scaling relationships
   - Memory scaling relationships
   - GC overhead analysis
   - Bottleneck identification

2. **Profiling data**: `pipeline_analysis/kstar/profiles/`
   - GC logs for each system size
   - CPU profiles
   - Memory profiles
   - Custom instrumentation logs

3. **Analysis scripts**: `scripts/analyze_kstar_profile.py`, `scripts/compare_workloads.py`
   - Automated metric extraction
   - Scaling relationship calculation
   - Visualization generation

## Timeline

- **Day 1**: Set up profiling infrastructure, run small system
- **Day 2**: Run medium system, start analysis
- **Day 3**: Run large system (may take 1-2 days), complete analysis
- **Day 4-5**: Document findings, create scaling report

**Total**: 3-5 days (depending on large system runtime)

## Success Criteria

- Clear scaling relationships documented
- Bottlenecks identified
- Optimization targets prioritized
- Data-driven implementation decisions possible

