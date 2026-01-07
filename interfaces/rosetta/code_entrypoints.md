# Rosetta: code entry points (interfaces and extension seams)

Snapshot GitHub pages + raw headers:.

Repo (upstream): `https://github.com/RosettaCommons/rosetta`

## Core object model (high-level interfaces to study)

- **Mover (operator / proposal step)**: the “state-transforming” interface used to build protocols.
- **ScoreFunction (objective)**: weighted sum of energy terms (“score terms”).
- **EnergyMethod (term plugin)**: individual scoring terms plugged into ScoreFunction.
- **Pose (state)**: carries structure/geometry + annotations; is the primary mutable state passed through protocols.

## Where these live in the Rosetta codebase (when/if cloned)

Rosetta is large, but the interfaces are stable in these areas:

- **Operators (protocols/moves)**: `source/src/protocols/`
  - Mover interfaces and common movers are typically under `protocols/moves/`
- **Scoring**: `source/src/core/scoring/`
  - ScoreFunction and score term plumbing
- **Pose/state**: `source/src/core/pose/`
  - Pose and structural data model
- **Energy term implementations**: mostly under `source/src/core/scoring/methods/` and term-specific submodules

## Local doc snapshots (available now)

- GitHub landing snapshot: `interfaces/rosetta/docs/github/page.txt`
- GitHub code-tree snapshots:
  - `interfaces/rosetta/docs/github_tree/protocols/page.txt`
  - `interfaces/rosetta/docs/github_tree/core_scoring/page.txt`
  - `interfaces/rosetta/docs/github_tree/core_pose/page.txt`
- Raw header snapshots (primary interfaces):
  - `interfaces/rosetta/docs/raw_headers/Mover_hh/page.txt`
  - `interfaces/rosetta/docs/raw_headers/ScoreFunction_hh/page.txt`
  - `interfaces/rosetta/docs/raw_headers/Pose_hh/page.txt`
  - `interfaces/rosetta/docs/raw_headers/EnergyMethod_hh/page.txt`
  - `interfaces/rosetta/docs/raw_headers/EnergyMethodCreator_hh/page.txt`

