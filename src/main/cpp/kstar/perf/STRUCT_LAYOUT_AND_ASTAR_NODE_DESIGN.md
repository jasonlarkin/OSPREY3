# Struct layout and A* node design (bytes, alignment, cache)

This document specifies struct-layout constraints and experiments for the A* hot path.

## Hardware model

### Bytes

The CPU loads/stores bytes at addresses. It does not understand structs. Struct layout exists as field offsets baked into instructions.

### Alignment and padding

Alignment is an ABI requirement. The compiler inserts padding to satisfy field alignment and overall object alignment.

Example:

```cpp
struct Bad {
  char a;      // 1
  // 7 padding
  double b;    // 8
  char c;      // 1
  // 7 padding (to align whole struct)
};             // sizeof == 24 (typical)

struct Good {
  double b;    // 8
  char a;      // 1
  char c;      // 1
  // 6 padding
};             // sizeof == 16 (typical)
```

`Bad` moves 50% more bytes than `Good` for the same payload. In hot loops, that is cache/bandwidth waste.

### Cache lines

Cache lines are typically 64B.
- Large objects force more cache lines per access.
- Objects that straddle cache-line boundaries increase traffic.

## Codebase target: A* hot path

Hot function: `osprey::kstar::AStarSearchFast<double>::expandInto` (fast A* benchmark, `case:3`).

`perf annotate` shows:
- min-reduction arithmetic is vectorized (packed FP + masked stores).
- storage management can be visible (sized `operator delete` on clear, `operator new` on growth).

Treat node representation and object lifetime as first-class performance parameters.

## Node representation facts (current)

`AStarNodeFast<T>` (`src/main/cpp/kstar/astar_node_fast.hpp`) contains:
- inline assignment array (common case),
- heap fallback pointer storage (rare case).

Even when `uses_heap == false`, the node payload still includes the heap fallback pointer and scalar fields. Effects:
- object size (cache footprint),
- bytes copied/moved per child node,
- destructor work when a `std::vector<AStarNodeFast<T>>` is cleared (even if the heap pointer is null in the common case).

### Measured layout (DWARF, GDB `ptype /o`)

The implementation currently produces the following layout for `osprey::kstar::AStarNodeFast<double>` (DWARF-enabled build, e.g. `build/cpp/kstar-perf`):

- total size: **80 bytes**
- `inline_assignments`: 32B
- heap fallback pointer: 8B (`int16_t*`)
- padding holes:
  - 3B hole after `uses_heap`
  - 4B padding at end (alignment)

The 80B payload is copied per child materialization (e.g. `out.emplace_back(node)` in `expandInto`) even when the heap fallback is not used.
Clearing `std::vector<AStarNodeFast<...>>` still executes element destructors, but the common case no longer embeds a `std::vector` subobject.

Cache-capacity back-of-envelope (node payload only; ignores other working-set components):

- L1D 48 KiB: \(48*1024 / 80 \approx 614\) nodes
- L2 1.25 MiB: \(1.25*1024*1024 / 80 \approx 16{,}384\) nodes
- L3 6 MiB: \(6*1024*1024 / 80 \approx 78{,}643\) nodes

## Roofline-style cache-spill analysis (method)

This analysis models performance transitions as working sets exceed cache levels, then verifies those transitions with counters.

### Working sets (what grows)

- **Search state**:
  - open set heap storage (heap items + heap container overhead)
  - node pool (all generated nodes)
  - scratch buffers in `AStarSearchFast` (fixed size; scales with max RC)
- **EnergyMatrix**:
  - one-body array
  - pairwise blocks (dominant bytes; accessed with mixed contiguous/strided patterns)

The “spill point” is the moment the active working set no longer fits in a given cache.

### Metrics to collect (Linux)

Use fixed benchmark case and sweep one axis at a time:

```bash
perf stat -e \
cycles,instructions,branches,branch-misses, \
L1-dcache-loads,L1-dcache-load-misses, \
LLC-loads,LLC-load-misses \
-- <benchmark command>
```

Derived signals:

- IPC $(= instructions / cycles)$
- L1 miss rate $(= L1-dcache-load-misses / L1-dcache-loads)$
- LLC miss rate $(= LLC-load-misses / LLC-loads)$

Transitions show:

- increasing miss rates (L1 then LLC) as a sweep exceeds cache capacity
- IPC collapsing when DRAM becomes the limiter

### Sweep design

Two independent sweeps isolate different effects:

- **Structure sweep**: vary `num_positions` and `max_rc` (synthetic EnergyMatrix) at a fixed `max_pops`
  - isolates EnergyMatrix block size and min-reduction loop behavior
- **Search sweep**: vary `max_pops` at fixed `(num_positions, max_rc)`
  - isolates open-set/pool growth and node-copy pressure

### SIMD profiling control (build-time)

SIMD intrinsics in hot loops are build-time gated to enable scalar vs SIMD profiling from identical sources:

- CMake option: `KSTAR_ENABLE_SIMD_INTRINSICS` (ON/OFF)
- Compile definition: `OSPREY_KSTAR_ENABLE_SIMD_INTRINSICS=1/0`

## Rules

### Rule 1: no heap-owning members in the common-case node

Do not embed `std::vector` (or any heap-owning member) inside the common-case node representation. Put heap storage in a separate type.

Implement one:
- split types: `AStarNodeInline` and `AStarNodeHeap`
- tagged union of representations (manual lifetime management; treat as high-risk until proven)

### Rule 2: pack the hot fields

Keep `g_score/h_score/f_score` contiguous and aligned. Pack small fields (`level`, flags) to minimize padding.

### Rule 3: measure layout

Record `sizeof`, `alignof`, and field offsets before and after changes.

Example:

```cpp
#include <cstddef>
#include <type_traits>

static_assert(alignof(MyHotStruct) >= alignof(double));
// Enforce triviality deliberately when needed:
// static_assert(std::is_trivially_destructible_v<MyHotStruct>);
```

Linux layout tools:
- `pahole -C AStarNodeFast <binary>` requires DWARF (`-g`) or BTF. The default perf build uses `-O3` only. Build a `RelWithDebInfo` tree (or add `-g`) before using `pahole`.
- `-Wpadded` (clang/gcc)

## Experiments (ordered)

Run each experiment behind a branch. Validate with correctness gate + `kstar.bench_astar_smoke`. Benchmark `case:3`.

### Experiment A: split inline vs heap node types

Objective: remove the `std::vector` subobject from the common-case node.

Steps:
- create `AStarNodeFastInline<T, InlineCap>` with only `std::array<int16_t, InlineCap>`
- use it when `num_positions <= InlineCap`
- use a separate heap node type for larger sizes

Measure:
- `BM_AStarFast_PopExpand/case:3` wall time
- `perf report` allocator/destructor share

### Experiment B: manual heap storage for heap nodes

Objective: reduce destructor/copy/move surface area for heap cases.

Steps:
- replace `std::vector<int16_t>` with `{int16_t* p; uint32_t n; uint32_t cap;}`
- allocate/free only when heap representation is active

Constraint: do not proceed without tests. This adds invariants and failure modes.

### Experiment C: field ordering/packing

Objective: shrink `sizeof(AStarNodeFast<double>)` and reduce cache pressure.

Steps:
- reorder fields to reduce padding
- pack flags
- narrow integer types where safe

Measure:
- `sizeof` before/after
- benchmark before/after

## Mapping to perf annotate

Vectorized arithmetic is present. Remaining cost is dominated by data movement and object lifetime:
- bytes copied per child node
- cache lines touched per pop/expand
- destructor/allocator traffic from child storage management
- pointer chasing from heap-owned subobjects

