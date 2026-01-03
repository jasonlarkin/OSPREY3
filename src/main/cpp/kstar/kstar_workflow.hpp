#pragma once

#include <concepts>
#include <cstdint>
#include <limits>

#include "energy_matrix.hpp"
#include "partition_function.hpp"

namespace osprey::kstar {

// A minimal end-to-end “K* workflow” for the Phase-1 C++ port:
// take three EnergyMatrix instances (protein/ligand/complex), compute their
// partition functions, then combine into a K* score in log10 space.
//
// This intentionally avoids ConfSpace integration for now (that is a separate
// layer), but provides a concrete workflow surface area for testing/benchmarking.

template<std::floating_point T>
struct KStarPfuncTriplet final {
    PartitionFunctionResult<T> protein;
    PartitionFunctionResult<T> ligand;
    PartitionFunctionResult<T> complex;
};

template<std::floating_point T>
struct KStarWorkflowResult final {
    KStarPfuncTriplet<T> pfuncs;

    // log10(K*) bounds and a representative point estimate.
    T log10_lower_bound = std::numeric_limits<T>::quiet_NaN();
    T log10_upper_bound = std::numeric_limits<T>::quiet_NaN();
    T log10_value = std::numeric_limits<T>::quiet_NaN();

    // Convenience: whether all three pfunc computations converged.
    bool converged = false;
};

template<std::floating_point T>
class KStarWorkflow final {
public:
    [[nodiscard]] KStarWorkflowResult<T> compute(
        const EnergyMatrix<T>& protein,
        const EnergyMatrix<T>& ligand,
        const EnergyMatrix<T>& complex,
        T epsilon,
        PartitionFunctionMethod method = PartitionFunctionMethod::AStar,
        typename PartitionFunction<T>::ComputeOptions pfunc_options = {}
    ) const;
};

} // namespace osprey::kstar

