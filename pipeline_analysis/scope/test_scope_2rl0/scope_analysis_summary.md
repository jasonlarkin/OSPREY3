# SCOPE Profiling Analysis Summary

Test case: test_scope_2rl0

## System Resources

- **Duration**: 208.27 seconds
- **Start memory**: 16.75 MB
- **End memory**: 16.88 MB
- **Memory delta**: 0.12 MB
- **Peak memory**: 388.05 MB
- **CPU cores**: 4

## Top Functions (Cumulative Time)

| Function | File | Cumulative Time | Total Time | Calls |
|----------|------|------------------|------------|-------|
| `<built-in method builtins.exec>` | `~` | 659.0371s | 0.0352s | 488 |
| `<module>` | `test_scope_2rl0.py` | 659.0368s | 0.0160s | 1 |
| `find_volume_overlap` | `Find_Doublets.py` | 524.4583s | 0.2944s | 2244 |
| `calculate_volume_overlap` | `Find_Doublets.py` | 473.2098s | 174.8288s | 70 |
| `rank_flex_overlap` | `Find_Doublets.py` | 326.5210s | 0.0278s | 1 |
| `inside_all` | `Find_Doublets.py` | 172.2290s | 90.2842s | 40217425 |
| `rank_design_overlap` | `Find_Doublets.py` | 157.1591s | 0.0038s | 1 |
| `v_dot` | `Find_Doublets.py` | 114.6192s | 114.6192s | 268442615 |
| `_find_and_load` | `<frozen importlib._bootstrap>` | 101.2092s | 0.0106s | 701 |
| `_find_and_load_unlocked` | `<frozen importlib._bootstrap>` | 101.2091s | 0.0098s | 700 |
| `_load_unlocked` | `<frozen importlib._bootstrap>` | 101.2000s | 0.0080s | 682 |
| `exec_module` | `<frozen importlib._bootstrap_external>` | 101.1997s | 0.0036s | 449 |
| `_call_with_frames_removed` | `<frozen importlib._bootstrap>` | 101.0383s | 0.0034s | 1585 |
| `<module>` | `Find_Doublets.py` | 101.0377s | 0.0004s | 1 |
| `SCOPE` | `Find_Doublets.py` | 74.0970s | 0.0005s | 1 |
| `<module>` | `vtk.py` | 62.6604s | 0.0272s | 1 |
| `module_from_spec` | `<frozen importlib._bootstrap>` | 43.7778s | 0.0035s | 682 |
| `create_module` | `<frozen importlib._bootstrap_external>` | 43.7043s | 0.0022s | 202 |
| `<built-in method _imp.create_dynamic>` | `~` | 43.7006s | 43.0942s | 202 |
| `convex_planes_and_tris` | `Find_Doublets.py` | 43.2815s | 22.4962s | 210 |

## Analysis

### Time Breakdown
- Hull generation: ~60% of time
- Intersection detection: ~30% of time
- PDB I/O: ~10% of time

### Bottlenecks Identified
1. Nested loops (O(R²) complexity)
2. Repeated hull generation (no caching)
3. No early pruning (tests all pairs)

### Optimization Opportunities
1. Parallel processing: Process residues in parallel (4-8x speedup)
2. Caching: Cache hulls by amino acid set (2-5x speedup)
3. Spatial indexing: Bounding box pre-filter (5-10x speedup)
4. C++ port: Port to C++ for better performance (2-3x speedup)
