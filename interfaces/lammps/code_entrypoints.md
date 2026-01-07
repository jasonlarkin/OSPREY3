# LAMMPS: code entry points (OO interfaces and extension seams)

Repo: `interfaces/lammps`

## Core object model (extension surfaces)

- **Pair (pair potentials / nonbonded terms)**: `interfaces/lammps/src/pair.h`
- **Fix (integration steps, constraints, thermostats, control hooks)**: `interfaces/lammps/src/fix.h`
- **Compute (observables / derived quantities)**: `interfaces/lammps/src/compute.h` (if needed; scan)
- **Modify (orchestrates Fix and Compute hooks over timesteps)**: `interfaces/lammps/src/modify.h`

## Structural pattern (high value)

- “Style” registry pattern: many concrete implementations live in `src/pair_*.cpp`, `src/fix_*.cpp`, `src/compute_*.cpp` and are selected by **string style names** from input scripts.
- Core engine owns a registry/list of active objects (e.g., `Modify` holds `Fix**` and `Compute**`) and calls hook methods at well-defined simulation phases.

## Docs build entry points (from DOC_INDEX)

- Developer docs: `https://docs.lammps.org/Developer.html`
- Doxygen config: `interfaces/lammps/doc/doxygen/Doxyfile.in`

