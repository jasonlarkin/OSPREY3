#ifndef OSPREY_KSTAR_PARTITION_FUNCTION_HPP
#define OSPREY_KSTAR_PARTITION_FUNCTION_HPP

#include <cstdint>
#include <concepts>
#include <limits>
#include <vector>
#include <queue>
#include <cstddef>
#include "sequence.hpp"

namespace osprey {
namespace kstar {

enum class PartitionFunctionMethod : std::uint8_t {
    AStar,
    GradientDescent
};

enum class AStarVariant : std::uint8_t {
    Baseline,
    Fast
};

// Forward declarations
template<std::floating_point T>
class ConfSpace;

template<std::floating_point T>
class EnergyMatrix;

template<std::floating_point T>
class AStarSearch;

/**
 * Result of partition function calculation.
 */
template<std::floating_point T>
struct PartitionFunctionResult {
    T lower_bound;      // log10 partition function lower bound
    T upper_bound;      // log10 partition function upper bound
    T delta;            // (upper - lower) / upper
    int64_t num_confs;  // Number of conformations explored
    bool converged;     // delta <= epsilon
};

/**
 * Optional trace step emitted during A* partition function computation.
 *
 * This is meant for interactive debugging/visualization. It is opt-in and has no
 * overhead unless enabled via ComputeOptions (trace_max_steps > 0 and trace_steps != nullptr).
 */
template<std::floating_point T>
struct PartitionFunctionTraceStep {
    int64_t iter = 0;
    int32_t level = 0;
    int64_t open_size = 0;
    int64_t num_confs_evaluated = 0;

    // Node scores at the popped node.
    T g_score = T(0);
    T h_score = T(0);
    T f_score = T(0);

    // Current bounds in log10 space (matching the public API format).
    T log10_q_lower = std::numeric_limits<T>::lowest();
    T log10_q_upper = std::numeric_limits<T>::lowest();
    T delta = T(1);
    bool converged = false;
    bool is_leaf = false;

    // Optional full assignment vector (length = num_positions, -1 for unassigned).
    // Captured only when ComputeOptions.trace_capture_assignments == true.
    std::vector<int32_t> assignments{};
};

/**
 * Partition function calculator using A* search.
 * 
 * Computes the partition function Q = sum(exp(-E/kT)) for a given sequence
 * and conformation space using epsilon-approximation.
 * 
 * For Phase 1, uses simplified algorithm:
 * - A* search to enumerate conformations
 * - EnergyMatrix to compute energies
 * - Boltzmann weighting: Q = sum(exp(-E/RT))
 * - Epsilon-approximation for convergence
 * 
 * Implementation uses log-space arithmetic to avoid overflow (OSPREY uses BigDecimal).
 * For exact matching with OSPREY, use PartitionFunctionMPFR (requires MPFR library).
 */
template<std::floating_point T>
class PartitionFunction {
public:
    struct ComputeOptions {
        // For tiny spaces, C++ can compute an exact result quickly by enumerating all conformations.
        // This is great for correctness/testing, but it can distort performance benchmarks that want
        // to compare search algorithms (A* vs GD) fairly.
        bool allow_exact_enumeration = true;

        // When method == AStar, choose which A* implementation to run.
        // Baseline is kept intact for profiling/tracking.
        AStarVariant astar_variant = AStarVariant::Baseline;

        // ---- Optional trace instrumentation (A* only) ----
        //
        // If trace_steps != nullptr and trace_max_steps > 0, A* will append trace steps
        // as it runs, up to trace_max_steps.
        //
        // NOTE: This is intended for debugging/visualization and is not performance-friendly.
        int64_t trace_max_steps = 0;
        bool trace_capture_assignments = false;
        std::vector<PartitionFunctionTraceStep<T>>* trace_steps = nullptr;
    };

    /**
     * Compute partition function using EnergyMatrix (no ConfSpace needed yet).
     * 
     * This is a simplified version for Phase 1 that uses pre-computed EnergyMatrix.
     * 
     * @param emat Energy matrix (pre-computed)
     * @param epsilon Accuracy parameter (0 < epsilon < 1)
     * @return Partition function result
     */
    [[nodiscard]] PartitionFunctionResult<T> compute(
        const EnergyMatrix<T>& emat,
        T epsilon
    );

    [[nodiscard]] PartitionFunctionResult<T> compute(
        const EnergyMatrix<T>& emat,
        T epsilon,
        PartitionFunctionMethod method
    );

    [[nodiscard]] PartitionFunctionResult<T> compute(
        const EnergyMatrix<T>& emat,
        T epsilon,
        PartitionFunctionMethod method,
        ComputeOptions options
    );
    
    /**
     * Compute partition function for a sequence (full version, requires ConfSpace).
     * 
     * This is an expensive computation (minutes per sequence). The [[nodiscard]]
     * attribute ensures callers don't accidentally ignore the result.
     * 
     * @param seq Sequence to compute partition function for
     * @param confspace Conformation space (read-only, shared)
     * @param epsilon Accuracy parameter (0 < epsilon < 1)
     * @return Partition function result
     */
    [[nodiscard]] PartitionFunctionResult<T> compute(
        const Sequence& seq,
        const ConfSpace<T>& confspace,
        T epsilon
    );
    
private:
    // Boltzmann constant * temperature (RT)
    // R ≈ 0.001987 kcal/(K·mol), T ≈ 298.15 K (room temperature)
    static constexpr T RT = T(0.001987 * 298.15);  // ≈ 0.592 kcal/mol
    
    /**
     * Compute Boltzmann weight: exp(-E/RT)
     */
    [[nodiscard]] static T boltzmannWeight(T energy) noexcept;
    
    /**
     * Compute log10 of Boltzmann weight: log10(exp(-E/RT)) = -E/(RT*ln(10))
     * 
     * This avoids overflow for large Q* values by working in log-space.
     */
    [[nodiscard]] static T log10BoltzmannWeight(T energy) noexcept;
    
    /**
     * Add two values in log10 space: log10(10^a + 10^b)
     * 
     * Numerically stable formula:
     *   log10(10^a + 10^b) = max(a, b) + log10(1 + 10^(min(a,b) - max(a,b)))
     */
    [[nodiscard]] static T log10Add(T log_a, T log_b) noexcept;

    /**
     * Subtract two values in log10 space: log10(10^a - 10^b)
     *
     * Requires a >= b and 10^a >= 10^b. If a==b, result is -inf (log10(0)).
     *
     * Numerically stable formula:
     *   log10(10^a - 10^b) = a + log10(1 - 10^(b-a))
     */
    [[nodiscard]] static T log10Sub(T log_a, T log_b) noexcept;
    
    /**
     * Compute partition function using A* search.
     */
    [[nodiscard]] PartitionFunctionResult<T> computeWithAStar(
        const EnergyMatrix<T>& emat,
        T epsilon,
        ComputeOptions options
    );

    [[nodiscard]] PartitionFunctionResult<T> computeWithGradientDescent(
        const EnergyMatrix<T>& emat,
        T epsilon,
        ComputeOptions options
    );

    /**
     * Compute partition function by exhaustive enumeration of all conformations.
     *
     * This is used as a correctness fallback for small spaces (eg, unit tests).
     * It returns an exact result (upper==lower, delta==0).
     */
    [[nodiscard]] PartitionFunctionResult<T> computeExactByEnumeration(
        const EnergyMatrix<T>& emat
    );
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_PARTITION_FUNCTION_HPP

