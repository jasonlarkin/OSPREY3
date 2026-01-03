# Benchmarking (C++ K* / A*)

This directory has **microbenchmarks** for:
- **Partition function** end-to-end (`kstar_partition_function_bench`, Google Benchmark)
- **A\* baseline vs fast** search hot-loop (`kstar_astar_search_bench`, Google Benchmark)
- **Partition function** chrono runner (`kstar_partition_function_chrono`, simple wall-clock)

Detailed benchmark outputs and profiling notes live under:
- `src/main/cpp/kstar/perf/` (start with `src/main/cpp/kstar/perf/README.md`)

All of these are built only when configuring with `-DKSTAR_ENABLE_BENCHMARKS=ON` (default is OFF).

## Build (configure once)

From repo root:

```bash
cmake -S src/main/cpp/kstar -B build/cpp/kstar -DKSTAR_ENABLE_BENCHMARKS=ON
cmake --build build/cpp/kstar -j
```

## Convenience run targets (CMake)

These targets run the benchmarks:

```bash
cmake --build build/cpp/kstar --target kstar_run_astar_search_bench
cmake --build build/cpp/kstar --target kstar_run_partition_function_bench
cmake --build build/cpp/kstar --target kstar_run_partition_function_chrono
```

## Run Google Benchmark binaries

From repo root:

```bash
./build/cpp/kstar/kstar_partition_function_bench --benchmark_min_time=0.5
./build/cpp/kstar/kstar_astar_search_bench --benchmark_min_time=0.5
```

Useful flags:

```bash
./build/cpp/kstar/kstar_astar_search_bench --benchmark_filter=BM_AStar.* --benchmark_min_time=1.0
./build/cpp/kstar/kstar_partition_function_bench --benchmark_filter=BM_Pfunc_AStar.* --benchmark_min_time=1.0
```

If test data resolution fails, set:

```bash
export OSPREY_KSTAR_TEST_DATA_DIR=/mnt/c/Users/denis/Documents/jobs_october_2025/ten63/osprey-fork_modern/build/cpp/kstar/test_data
```

## Run the chrono benchmark

The chrono runner prints per-case timings and supports `--reps=<N>`:

```bash
./build/cpp/kstar/kstar_partition_function_chrono --reps=3
```

## Benchmark smoke tests (CTest; optional)

If configured with `-DKSTAR_ENABLE_BENCHMARKS=ON`, there are very short-running benchmark smoke tests:

```bash
ctest --test-dir build/cpp/kstar --output-on-failure -R kstar\\.bench_
```

Labels:

- `kstar;benchmark;synthesized;smoke;astar`
- `kstar;benchmark;synthesized;smoke;partition_function`

## perf (Linux / WSL)

Quick counters:

```bash
perf stat -d -- ./build/cpp/kstar/kstar_astar_search_bench --benchmark_filter=BM_AStar.* --benchmark_min_time=0.5
```

Profile + report:

```bash
perf record -g -- ./build/cpp/kstar/kstar_astar_search_bench --benchmark_filter=BM_AStar.* --benchmark_min_time=0.5
perf report
```

## valgrind / callgrind

Callgrind is slow; keep benchmark time small:

```bash
valgrind --tool=callgrind --callgrind-out-file=callgrind.astar.out \
  ./build/cpp/kstar/kstar_astar_search_bench --benchmark_filter=BM_AStar.* --benchmark_min_time=0.01

callgrind_annotate callgrind.astar.out | head -200
```

