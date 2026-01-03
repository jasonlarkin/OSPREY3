#ifndef OSPREY_KSTAR_PARALLEL_HPP
#define OSPREY_KSTAR_PARALLEL_HPP

#include <vector>
#include <thread>
#include <future>
#include <atomic>
#include <mutex>
#include <concepts>
#include "sequence.hpp"
#include "partition_function.hpp"
#include "thread_pool.hpp"

namespace osprey {

// Forward declaration
template<std::floating_point T>
class ConfSpace;

// Import kstar namespace types
using kstar::PartitionFunctionResult;
using kstar::PartitionFunction;

/**
 * K* score computed from partition functions.
 */
template<std::floating_point T>
struct KStarScore {
    T log10_value;           // log10(K*)
    T log10_lower_bound;     // Lower bound
    T log10_upper_bound;     // Upper bound
    
    [[nodiscard]] static KStarScore from_pfuncs(
        const PartitionFunctionResult<T>& protein,
        const PartitionFunctionResult<T>& ligand,
        const PartitionFunctionResult<T>& complex
    ) noexcept;
};

/**
 * Scored sequence with K* score.
 */
template<std::floating_point T>
struct ScoredSequence {
    Sequence sequence;
    KStarScore<T> score;
};

/**
 * Settings for K* parallel computation.
 */
template<std::floating_point T>
struct KStarSettings {
    T epsilon = T(0.99);
    int num_threads = static_cast<int>(std::thread::hardware_concurrency());
    bool show_progress = true;
};

/**
 * Parallel K* implementation using C++20.
 * 
 * Processes sequences in parallel using a thread pool.
 */
template<std::floating_point T>
class KStarParallel {
public:
    /**
     * Compute K* scores for multiple sequences in parallel.
     * 
     * This is an expensive computation (hours for many sequences). The [[nodiscard]]
     * attribute ensures callers don't accidentally ignore the results.
     * 
     * @param sequences Sequences to process
     * @param protein_confspace Protein conformation space (read-only, shared)
     * @param ligand_confspace Ligand conformation space (read-only, shared)
     * @param complex_confspace Complex conformation space (read-only, shared)
     * @param settings Computation settings
     * @return Vector of scored sequences
     */
    [[nodiscard]] std::vector<ScoredSequence<T>> compute(
        const std::vector<Sequence>& sequences,
        const ConfSpace<T>& protein_confspace,
        const ConfSpace<T>& ligand_confspace,
        const ConfSpace<T>& complex_confspace,
        const KStarSettings<T>& settings = {}
    );
};

} // namespace osprey

#endif // OSPREY_KSTAR_PARALLEL_HPP

