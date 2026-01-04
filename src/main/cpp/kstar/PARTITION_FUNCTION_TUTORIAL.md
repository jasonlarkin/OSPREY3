# PartitionFunction tutorial: design decisions and what they imply

This tutorial describes the key design choices in the current C++ `PartitionFunction` port and the behaviors we intentionally lock down with tests.

## Concepts

- **Q**: partition function $Q = \sum_{\text{conf}} \exp(-E/RT)$
- **log10-space**: we store bounds as `log10(Q)` to avoid overflow.
- **Bounds**: `PartitionFunctionResult<T>::lower_bound` and `upper_bound` are both `log10(Q)` values.

## Decision: no pruning in A* partition function

When enumerating conformations with A*, **do not** prune high-energy nodes based on the current best energy.

Reason:
- Pruning changes the mathematical quantity: it turns Q into a truncated sum, which generally **underestimates** Q and can break bound logic (including prematurely emptying the open set).

Where:
- `PartitionFunction<T>::computeWithAStar` (see comments near the child expansion loop).

Test strategy:
- Use `epsilon = 0` and `allow_exact_enumeration = false` on a small space to force full enumeration through the A* path, then validate `num_confs == total_confs` and `delta == 0`.
- Add a tutorial test that demonstrates the failure mode if pruning is introduced:
  - `PartitionFunction_Tutorial.PruningUnderestimatesQ_ComparedToExactOracle`

## Decision: exact enumeration shortcut for small spaces

For very small spaces, we optionally bypass A*/GD and compute Q exactly by enumerating all conformations.

Reason:
- Provides a correctness anchor for unit tests and tiny toy examples.
- Avoids wasting effort comparing algorithms on spaces where “search” overhead dominates.

Where:
- `PartitionFunction<T>::compute` gate: `ComputeOptions::allow_exact_enumeration` + method `AStar`.
- `PartitionFunction<T>::computeExactByEnumeration`.

Test strategy:
- When `allow_exact_enumeration = true` and the space is below the threshold, expect:
  - `converged == true`
  - `delta == 0`
  - `lower_bound == upper_bound`
  - `num_confs == total_confs`

## Decision: score/energy split invariant (Gradient Descent method)

The GradientDescent method is implemented as a two-reader process over a single `ConfSearch`:
- a “score” stream reads ahead to tighten an upper bound cheaply
- an “energy” stream consumes buffered scored conformations to tighten the lower bound

Key invariant:
- scoring must stay sufficiently ahead so the splitter math stays valid.

Where:
- `PartitionFunction<T>::computeWithGradientDescent` (see comments around step selection and buffer safety).

Test strategy:
- Targeted tutorial tests exist to lock down control-loop edge cases:
  - `PartitionFunction_Tutorial_GD.NoConformations_EarlyReturnNegInfBounds`
  - `PartitionFunction_Tutorial_GD.SingleConformation_ConvergesExact`
  - `PartitionFunction_Tutorial_GD.CantMakeProgressWhenRemainingIsPosInf_ReturnsNonConverged`

## How to run the tutorial tests

```bash
ctest --test-dir build/cpp/kstar --output-on-failure -R '^PartitionFunction_Tutorial\\.'
ctest --test-dir build/cpp/kstar --output-on-failure -R '^PartitionFunction_Tutorial_GD\\.'
```

