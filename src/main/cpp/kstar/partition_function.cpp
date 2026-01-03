#include "partition_function.hpp"
#include "astar_search.hpp"
#include "astar_node.hpp"
#include "astar_search_fast.hpp"
#include "astar_node_fast.hpp"
#include "energy_matrix.hpp"
#include "log_space.hpp"
#include <cmath>
#include <limits>
#include <algorithm>
#include <chrono>
#include <deque>
#include <iostream>
#include <iomanip>

#ifndef OSPREY_KSTAR_PFUNC_DEBUG
#define OSPREY_KSTAR_PFUNC_DEBUG 0
#endif

namespace osprey {
namespace kstar {

static constexpr bool kPfuncDebug = (OSPREY_KSTAR_PFUNC_DEBUG != 0);

template<std::floating_point T>
[[nodiscard]] T PartitionFunction<T>::boltzmannWeight(T energy) noexcept {
    // Boltzmann weight: exp(-E/RT)
    // Handle edge cases
    if (std::isinf(energy)) {
        if (energy > T(0)) {
            return T(0);  // exp(-inf) = 0
        } else {
            return std::numeric_limits<T>::max();  // exp(+inf) = inf
        }
    }
    if (std::isnan(energy)) {
        return T(0);
    }
    
    return std::exp(-energy / RT);
}

template<std::floating_point T>
[[nodiscard]] T PartitionFunction<T>::log10BoltzmannWeight(T energy) noexcept {
    // log10(exp(-E/RT)) = -E/(RT*ln(10))
    // Handle edge cases
    if (std::isinf(energy)) {
        if (energy > T(0)) {
            return std::numeric_limits<T>::lowest();  // log10(0) = -inf
        } else {
            return std::numeric_limits<T>::max();  // log10(inf) = inf
        }
    }
    if (std::isnan(energy)) {
        return std::numeric_limits<T>::quiet_NaN();
    }
    
    // log10(exp(-E/RT)) = (-E/RT) / ln(10) = -E/(RT*ln(10))
    // Note: For negative energies (low = good), this gives positive log10 values
    static constexpr T ln10 = T(2.3025850929940459);
    return -energy / (RT * ln10);
}

template<std::floating_point T>
[[nodiscard]] T PartitionFunction<T>::log10Add(T log_a, T log_b) noexcept {
    return logspace::log10Add(log_a, log_b);
}

template<std::floating_point T>
[[nodiscard]] T PartitionFunction<T>::log10Sub(T log_a, T log_b) noexcept {
    return logspace::log10Sub(log_a, log_b);
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::computeWithAStar(
    const EnergyMatrix<T>& emat,
    T epsilon,
    ComputeOptions options
) {
    PartitionFunctionResult<T> result;
    
    int32_t num_positions = emat.getNumPositions();
    std::vector<int32_t> num_confs_per_pos(num_positions);
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        num_confs_per_pos[pos] = emat.getNumConfsAtPos(pos);
    }

    // Fast A* variant (kept separate from baseline for profiling/tracking).
    if (options.astar_variant == AStarVariant::Fast) {
        // Create A* search
        AStarSearchFast<T> astar(emat, num_positions, num_confs_per_pos);

        struct NodeMinScore {
            bool operator()(const AStarNodeFast<T>& a, const AStarNodeFast<T>& b) const noexcept {
                return a.getScore() > b.getScore();
            }
        };
        std::priority_queue<AStarNodeFast<T>, std::vector<AStarNodeFast<T>>, NodeMinScore> open_set;

        auto root = AStarNodeFast<T>::root(num_positions);
        root.g_score = astar.computeGScore(root);
        root.h_score = astar.computeHScore(root);
        open_set.push(root);

        T log10_q_lower = std::numeric_limits<T>::lowest();
        T log10_q_upper = std::numeric_limits<T>::lowest();

        int64_t num_confs_evaluated = 0;
        T best_upper_bound = std::numeric_limits<T>::max();

        int64_t total_confs = 1;
        for (int32_t pos = 0; pos < num_positions; ++pos) {
            total_confs *= num_confs_per_pos[pos];
        }

        while (!open_set.empty()) {
            AStarNodeFast<T> node = open_set.top();
            open_set.pop();

            if (astar.isLeaf(node)) {
                T energy = astar.computeGScore(node);
                T log10_weight = log10BoltzmannWeight(energy);

                if (log10_q_lower == std::numeric_limits<T>::lowest()) {
                    log10_q_lower = log10_weight;
                } else {
                    log10_q_lower = log10Add(log10_q_lower, log10_weight);
                }
                num_confs_evaluated++;

                if (best_upper_bound == std::numeric_limits<T>::max()) {
                    best_upper_bound = energy;
                } else {
                    best_upper_bound = std::min(best_upper_bound, energy);
                }
            } else {
                auto children = astar.expand(node);
                for (const auto& child : children) {
                    open_set.push(child);
                }
            }

            int64_t remaining_confs = total_confs - num_confs_evaluated;
            T log10_remaining_estimate = std::numeric_limits<T>::lowest();

            if (remaining_confs > 0) {
                if (!open_set.empty()) {
                    T best_score = open_set.top().getScore();
                    T log10_max_weight = log10BoltzmannWeight(best_score);
                    T log10_N = std::log10(static_cast<T>(remaining_confs));
                    log10_remaining_estimate = log10_max_weight + log10_N;
                } else {
                    if (best_upper_bound < std::numeric_limits<T>::max()) {
                        T log10_max_weight = log10BoltzmannWeight(best_upper_bound);
                        T log10_N = std::log10(static_cast<T>(remaining_confs));
                        log10_remaining_estimate = log10_max_weight + log10_N;
                    }
                }
            }

            if (log10_q_lower > std::numeric_limits<T>::lowest() &&
                log10_remaining_estimate > std::numeric_limits<T>::lowest()) {
                log10_q_upper = log10Add(log10_q_lower, log10_remaining_estimate);
            } else if (log10_q_lower > std::numeric_limits<T>::lowest()) {
                log10_q_upper = log10_q_lower;
            }

            if (log10_q_upper > std::numeric_limits<T>::lowest() &&
                log10_q_lower > std::numeric_limits<T>::lowest()) {
                T diff_log = log10_q_lower - log10_q_upper;
                if (diff_log < T(-36)) {
                    result.delta = T(1);
                } else {
                    result.delta = T(1) - std::pow(T(10), diff_log);
                }
                if (result.delta <= epsilon) {
                    result.converged = true;
                    break;
                }
            }
        }

        int64_t remaining_confs = total_confs - num_confs_evaluated;
        if (remaining_confs > 0 && log10_q_upper == std::numeric_limits<T>::lowest()) {
            T log10_max_weight = log10BoltzmannWeight(best_upper_bound);
            T log10_N = std::log10(static_cast<T>(remaining_confs));
            T log10_remaining = log10_max_weight + log10_N;
            log10_q_upper = log10Add(log10_q_lower, log10_remaining);
        }

        result.lower_bound = log10_q_lower;
        result.upper_bound = (log10_q_upper > std::numeric_limits<T>::lowest()) ? log10_q_upper : log10_q_lower;
        result.num_confs = num_confs_evaluated;

        if (log10_q_upper > std::numeric_limits<T>::lowest() &&
            log10_q_lower > std::numeric_limits<T>::lowest()) {
            T diff_log = log10_q_lower - log10_q_upper;
            if (diff_log < T(-36)) {
                result.delta = T(1);
            } else {
                result.delta = T(1) - std::pow(T(10), diff_log);
            }
            result.converged = (result.delta <= epsilon);
        } else {
            result.delta = T(1);
            result.converged = false;
        }

        return result;
    }
    
    // Create A* search
    AStarSearch<T> astar(emat, num_positions, num_confs_per_pos);
    
    // Priority queue for open set (min-heap: lower f=g+h score = higher priority)
    // IMPORTANT: do not rely on overloaded comparison operators here; they are easy to get wrong.
    struct NodeMinScore {
        bool operator()(const AStarNode<T>& a, const AStarNode<T>& b) const noexcept {
            return a.getScore() > b.getScore(); // higher score = lower priority
        }
    };
    std::priority_queue<AStarNode<T>, std::vector<AStarNode<T>>, NodeMinScore> open_set;
    
    // Start with root node
    auto root = AStarNode<T>::root(num_positions);
    root.g_score = astar.computeGScore(root);
    root.h_score = astar.computeHScore(root);
    open_set.push(root);
    
    // Partition function bounds (in log10 space to avoid overflow)
    // OSPREY uses BigDecimal (arbitrary precision), C++ uses log-space arithmetic
    T log10_q_lower = std::numeric_limits<T>::lowest();  // log10 lower bound: log10(sum(exp(-E/RT)))
    T log10_q_upper = std::numeric_limits<T>::lowest();  // log10 upper bound
    
    int64_t num_confs_evaluated = 0;
    T best_upper_bound = std::numeric_limits<T>::max();
    
    // Compute total number of conformations
    // OSPREY: RCs.getNumConformations() multiplies numRCs for each position
    int64_t total_confs = 1;
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        total_confs *= num_confs_per_pos[pos];
    }
    
    // DEBUG: Print total_confs and num_confs_per_pos
    if constexpr (kPfuncDebug) {
        std::cerr << "[DEBUG] Partition function computation:\n";
        std::cerr << "  num_positions: " << num_positions << "\n";
        std::cerr << "  num_confs_per_pos: [";
        for (int32_t pos = 0; pos < num_positions; ++pos) {
            std::cerr << num_confs_per_pos[pos];
            if (pos < num_positions - 1) std::cerr << ", ";
        }
        std::cerr << "]\n";
        std::cerr << "  total_confs: " << total_confs;
        if (total_confs > 1e15) {
            std::cerr << " (WARNING: suspiciously large!)";
        }
        std::cerr << "\n";
    }
    
    // A* search loop
    while (!open_set.empty()) {
        // Get best node
        AStarNode<T> node = open_set.top();
        open_set.pop();
        
        // If leaf node, evaluate energy and update bounds
        if (astar.isLeaf(node)) {
            T energy = astar.computeGScore(node);  // Full energy for complete conformation
            T log10_weight = log10BoltzmannWeight(energy);
            
            // DEBUG: Print first few conformations
            if constexpr (kPfuncDebug) if (num_confs_evaluated < 5) {
                std::cerr << "  [DEBUG] Conf #" << num_confs_evaluated << ": energy=" << energy 
                          << ", log10_weight=" << log10_weight << "\n";
            }
            
            // Add in log-space: log10_q_lower = log10(10^log10_q_lower + 10^log10_weight)
            if (log10_q_lower == std::numeric_limits<T>::lowest()) {
                // First weight: initialize
                log10_q_lower = log10_weight;
            } else {
                T old_log10_q_lower = log10_q_lower;
                log10_q_lower = log10Add(log10_q_lower, log10_weight);
                
                // DEBUG: Trace suspicious jumps
                if constexpr (kPfuncDebug) if (num_confs_evaluated < 5) {
                    std::cerr << "    log10_q_lower: " << old_log10_q_lower << " -> " << log10_q_lower << "\n";
                }
                if constexpr (kPfuncDebug) if (log10_q_lower > T(50) && old_log10_q_lower < T(10)) {
                    std::cerr << "  [WARNING] Sudden jump in log10_q_lower: " << old_log10_q_lower 
                              << " -> " << log10_q_lower << " (conf #" << num_confs_evaluated << ")\n";
                }
            }
            num_confs_evaluated++;
            
            // Update best upper bound (use actual energy, not g+h)
            if (best_upper_bound == std::numeric_limits<T>::max()) {
                best_upper_bound = energy;
            } else {
                best_upper_bound = std::min(best_upper_bound, energy);
            }
        } else {
            // Expand node
            auto children = astar.expand(node);
            for (const auto& child : children) {
                // NOTE: Do NOT prune for partition function.
                // Partition function needs the sum over *all* conformations; high-energy confs
                // may contribute negligibly, but pruning based on current best energy will
                // generally under-estimate Q* and can empty the open set prematurely.
                open_set.push(child);
            }
        }
        
        // Compute upper bound estimate (in log10 space)
        // Conservative estimate: log10(q_lower + remaining_confs * max_weight)
        int64_t remaining_confs = total_confs - num_confs_evaluated;
        T log10_remaining_estimate = std::numeric_limits<T>::lowest();
        
        if (remaining_confs > 0) {
            if (!open_set.empty()) {
                // Estimate based on best node's score (lower bound on remaining energies)
                T best_score = open_set.top().getScore();
                T log10_max_weight = log10BoltzmannWeight(best_score);
                // Conservative: assume all remaining conformations have at least this weight
                // log10(N * w) = log10(N) + log10(w)
                T log10_N = std::log10(static_cast<T>(remaining_confs));
                log10_remaining_estimate = log10_max_weight + log10_N;
                
                // DEBUG: Print upper bound estimate details
                if constexpr (kPfuncDebug) if (num_confs_evaluated <= 5 || log10_N > T(50)) {
                    std::cerr << "  [DEBUG] Upper bound estimate (iteration " << num_confs_evaluated << "):\n";
                    std::cerr << "    remaining_confs: " << remaining_confs << "\n";
                    std::cerr << "    log10_N: " << log10_N << "\n";
                    std::cerr << "    best_score: " << best_score << "\n";
                    std::cerr << "    log10_max_weight: " << log10_max_weight << "\n";
                    std::cerr << "    log10_remaining_estimate: " << log10_remaining_estimate << "\n";
                }
            } else {
                // Open set is empty but we haven't evaluated all conformations
                // Use best_upper_bound (best energy seen so far) as estimate
                if (best_upper_bound < std::numeric_limits<T>::max()) {
                    T log10_max_weight = log10BoltzmannWeight(best_upper_bound);
                    T log10_N = std::log10(static_cast<T>(remaining_confs));
                    log10_remaining_estimate = log10_max_weight + log10_N;
                    
                    // DEBUG: Print upper bound estimate details
                    if constexpr (kPfuncDebug) if (num_confs_evaluated <= 5 || log10_N > T(50)) {
                        std::cerr << "  [DEBUG] Upper bound estimate (empty open_set, iteration " << num_confs_evaluated << "):\n";
                        std::cerr << "    remaining_confs: " << remaining_confs << "\n";
                        std::cerr << "    log10_N: " << log10_N << "\n";
                        std::cerr << "    best_upper_bound: " << best_upper_bound << "\n";
                        std::cerr << "    log10_max_weight: " << log10_max_weight << "\n";
                        std::cerr << "    log10_remaining_estimate: " << log10_remaining_estimate << "\n";
                    }
                }
            }
        }
        
        // Upper bound: log10(q_lower + remaining_estimate)
        // Only compute if we have both values
        if (log10_q_lower > std::numeric_limits<T>::lowest() && 
            log10_remaining_estimate > std::numeric_limits<T>::lowest()) {
            // CRITICAL: Only compute upper bound, never modify lower bound here
            log10_q_upper = log10Add(log10_q_lower, log10_remaining_estimate);
        } else if (log10_q_lower > std::numeric_limits<T>::lowest()) {
            log10_q_upper = log10_q_lower;
        }
        
        // Check convergence (in log-space)
        // delta = (q_upper - q_lower) / q_upper
        // In log-space: delta = 1 - 10^(log10_q_lower - log10_q_upper)
        if (log10_q_upper > std::numeric_limits<T>::lowest() && 
            log10_q_lower > std::numeric_limits<T>::lowest()) {
            T diff_log = log10_q_lower - log10_q_upper;
            if (diff_log < T(-36)) {
                // q_lower << q_upper, delta ≈ 1
                result.delta = T(1);
            } else {
                // delta = 1 - 10^(log10_q_lower - log10_q_upper)
                result.delta = T(1) - std::pow(T(10), diff_log);
            }
            if (result.delta <= epsilon) {
                result.converged = true;
                break;
            }
        }
    }
    
    // DEBUG: Check log10_q_lower after loop
    if constexpr (kPfuncDebug) {
        std::cerr << "  [DEBUG] After main loop:\n";
        std::cerr << "    log10_q_lower: " << log10_q_lower << "\n";
        std::cerr << "    log10_q_upper: " << log10_q_upper << "\n";
        std::cerr << "    num_confs_evaluated: " << num_confs_evaluated << "\n";
    }
    
    // If open_set is empty but we haven't evaluated all conformations, 
    // we need to update q_upper one more time
    int64_t remaining_confs = total_confs - num_confs_evaluated;
    if (remaining_confs > 0 && log10_q_upper == std::numeric_limits<T>::lowest()) {
        // Use best_upper_bound as conservative estimate
        T log10_max_weight = log10BoltzmannWeight(best_upper_bound);
        T log10_N = std::log10(static_cast<T>(remaining_confs));
        T log10_remaining = log10_max_weight + log10_N;
        
        // DEBUG: Check for suspicious values before computing upper bound
        if constexpr (kPfuncDebug) {
            std::cerr << "  [DEBUG] Final upper bound update:\n";
            std::cerr << "    log10_q_lower (before): " << log10_q_lower << "\n";
            std::cerr << "    log10_remaining: " << log10_remaining << "\n";
            std::cerr << "    log10_N: " << log10_N << "\n";
        }
        
        log10_q_upper = log10Add(log10_q_lower, log10_remaining);
        
        // DEBUG: Verify log10_q_lower wasn't modified
        if constexpr (kPfuncDebug) {
            std::cerr << "    log10_q_lower (after): " << log10_q_lower << "\n";
            std::cerr << "    log10_q_upper: " << log10_q_upper << "\n";
        }
    }
    
    // Results are already in log10 space (matching OSPREY's format)
    // CRITICAL: Ensure we're returning the lower bound, not the upper bound
    // The lower bound is the sum of evaluated conformations only
    result.lower_bound = log10_q_lower;
    result.upper_bound = (log10_q_upper > std::numeric_limits<T>::lowest()) ? log10_q_upper : log10_q_lower;
    result.num_confs = num_confs_evaluated;
    
    // DEBUG: Print final results
    if constexpr (kPfuncDebug) {
        std::cerr << "  [DEBUG] Final results:\n";
        std::cerr << "    num_confs_evaluated: " << num_confs_evaluated << " / " << total_confs << "\n";
        std::cerr << "    log10_q_lower: " << log10_q_lower << "\n";
        std::cerr << "    log10_q_upper: " << result.upper_bound << "\n";
        std::cerr << "    Q* (10^log10_q_lower): " << std::pow(T(10), log10_q_lower) << "\n";
    }
    
    // Final delta calculation
    if (log10_q_upper > std::numeric_limits<T>::lowest() && 
        log10_q_lower > std::numeric_limits<T>::lowest()) {
        T diff_log = log10_q_lower - log10_q_upper;
        if (diff_log < T(-36)) {
            result.delta = T(1);
        } else {
            result.delta = T(1) - std::pow(T(10), diff_log);
        }
        result.converged = (result.delta <= epsilon);
    } else {
        result.delta = T(1);
        result.converged = false;
    }
    
    return result;
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::computeExactByEnumeration(
    const EnergyMatrix<T>& emat
) {

    PartitionFunctionResult<T> result;
    result.converged = true;
    result.delta = T(0);

    const int32_t num_positions = emat.getNumPositions();
    std::vector<int32_t> num_confs_per_pos(num_positions);

    int64_t total_confs = 1;
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        num_confs_per_pos[pos] = emat.getNumConfsAtPos(pos);
        total_confs *= static_cast<int64_t>(num_confs_per_pos[pos]);
    }

    // Enumerate all conformations via mixed-radix counter
    std::vector<int32_t> conf(num_positions, 0);

    T log10_q = std::numeric_limits<T>::lowest();
    for (int64_t i = 0; i < total_confs; ++i) {
        T energy = emat.computeEnergy(conf);
        T log10_w = log10BoltzmannWeight(energy);
        if (log10_q == std::numeric_limits<T>::lowest()) {
            log10_q = log10_w;
        } else {
            log10_q = log10Add(log10_q, log10_w);
        }

        // increment conf
        for (int32_t pos = 0; pos < num_positions; ++pos) {
            conf[pos]++;
            if (conf[pos] < num_confs_per_pos[pos]) {
                break;
            }
            conf[pos] = 0;
        }
    }

    result.lower_bound = log10_q;
    result.upper_bound = log10_q;
    result.num_confs = total_confs;
    return result;
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::compute(
    const EnergyMatrix<T>& emat,
    T epsilon
) {
    return compute(emat, epsilon, PartitionFunctionMethod::AStar, ComputeOptions{});
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::computeWithGradientDescent(
    const EnergyMatrix<T>& emat,
    T epsilon
) {
    // CPU-only port of OSPREY's edu.duke.cs.osprey.kstar.pfunc.GradientDescentPfunc
    //
    // Key semantic: split a single ConfSearch into two readers where the "score" reader
    // always reads ahead, and the "energy" reader consumes the buffered scored confs.
    // This allows tightening the upper bound using many scores cheaply, and tightening
    // the lower bound using (more expensive) energies. In OSPREY, "score" is a lower
    // bound and "energy" is a minimized energy; in this Phase-1 C++ port, both are
    // derived from the EnergyMatrix (no minimization), but we preserve the bound math.

    PartitionFunctionResult<T> result;
    result.converged = false;
    result.delta = T(1);
    result.num_confs = 0;

    const int32_t num_positions = emat.getNumPositions();
    std::vector<int32_t> num_confs_per_pos(num_positions);
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        num_confs_per_pos[pos] = emat.getNumConfsAtPos(pos);
    }

    // Build the A* machinery used to enumerate conformations in increasing score
    AStarSearch<T> astar(emat, num_positions, num_confs_per_pos);
    struct NodeMinScore {
        bool operator()(const AStarNode<T>& a, const AStarNode<T>& b) const noexcept {
            return a.getScore() > b.getScore();
        }
    };
    std::priority_queue<AStarNode<T>, std::vector<AStarNode<T>>, NodeMinScore> open_set;
    {
        auto root = AStarNode<T>::root(num_positions);
        root.g_score = astar.computeGScore(root);
        root.h_score = astar.computeHScore(root);
        open_set.push(root);
    }

    // Total number of conformations: compute exact when it fits, also track log10(total)
    bool total_fits_u64 = true;
    std::uint64_t total_confs_u64 = 1;
    long double log10_total_confs = 0.0L;
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        const std::uint64_t m = static_cast<std::uint64_t>(num_confs_per_pos[pos]);
        if (m == 0) {
            total_fits_u64 = true;
            total_confs_u64 = 0;
            log10_total_confs = -std::numeric_limits<long double>::infinity();
            break;
        }
        if (total_fits_u64) {
            if (total_confs_u64 > std::numeric_limits<std::uint64_t>::max() / m) {
                total_fits_u64 = false;
            } else {
                total_confs_u64 *= m;
            }
        }
        log10_total_confs += std::log10(static_cast<long double>(m));
    }

    struct ScoredLeaf {
        AStarNode<T> node;
        T score;
    };

    auto next_scored_leaf = [&]() -> std::optional<ScoredLeaf> {
        while (!open_set.empty()) {
            AStarNode<T> node = open_set.top();
            open_set.pop();
            if (astar.isLeaf(node)) {
                // ensure g/h are populated (children have these precomputed in expand())
                if (node.g_score == T(0) && node.h_score == T(0) && num_positions > 0) {
                    node.g_score = astar.computeGScore(node);
                    node.h_score = astar.computeHScore(node);
                }
                return ScoredLeaf{std::move(node), node.getScore()};
            }
            auto children = astar.expand(node);
            for (const auto& child : children) {
                open_set.push(child);
            }
        }
        return std::nullopt;
    };

    // Splitter buffer: score-reader pushes, energy-reader pops (score must be ahead)
    std::deque<ScoredLeaf> buf;

    // State in log10 space (initialize to log10(0) = -inf)
    const T neg_inf = std::numeric_limits<T>::lowest();
    const T pos_inf = std::numeric_limits<T>::max();

    std::uint64_t num_scored = 0;
    std::uint64_t num_energied = 0;
    T log10_upper_score_sum = neg_inf;
    T log10_min_upper_score_w = pos_inf; // min weight => min log10(weight)
    T log10_lower_score_sum = neg_inf;
    T log10_energy_sum = neg_inf;
    T log10_min_lower_score_w = pos_inf;

    // slope heuristics (ported from Java, but with real timing)
    double scoreOps = 100.0;
    double energyOps = 1.0;
    double prevDelta = 1.0;
    double dEnergy = -1.0;
    double dScore = -1.0;

    auto calcSlope = [](double delta, double prevDeltaLocal, double otherSlope) -> double {
        double slope = delta - prevDeltaLocal; // should be <= 0
        if (slope >= 0.0) {
            slope = otherSlope / 10.0;
        }
        return slope;
    };

    auto log10_num_remaining_from_num_scored = [&](std::uint64_t scoredCount) -> T {
        if (total_fits_u64) {
            if (scoredCount >= total_confs_u64) {
                return neg_inf; // log10(0)
            }
            std::uint64_t rem = total_confs_u64 - scoredCount;
            return static_cast<T>(std::log10(static_cast<long double>(rem)));
        }
        // If we can't represent exact total, scoredCount is negligible relative to total
        return static_cast<T>(log10_total_confs);
    };

    auto get_upper_bound_log10 = [&]() -> T {
        // UB = (N - numScored)*minUpper + upperScoreSum - lowerScoreSum + energySum
        // In log10: UB = log10Add(unscored, upperScoreSum) then replace:
        // subtract lowerScoreSum, add energySum.
        if (num_scored == 0) {
            return pos_inf;
        }
        if (log10_min_upper_score_w == pos_inf) {
            return pos_inf;
        }

        T log10_unscored = neg_inf;
        {
            T log10_rem = log10_num_remaining_from_num_scored(num_scored);
            if (log10_rem > neg_inf && log10_min_upper_score_w > neg_inf) {
                log10_unscored = log10_rem + log10_min_upper_score_w;
            }
        }

        T ub = log10Add(log10_unscored, log10_upper_score_sum);

        // Replace score weights with energy weights for energied confs
        if (log10_lower_score_sum > neg_inf) {
            ub = log10Sub(ub, log10_lower_score_sum);
        }
        if (log10_energy_sum > neg_inf) {
            ub = log10Add(ub, log10_energy_sum);
        }
        return ub;
    };

    auto get_lower_bound_log10 = [&]() -> T {
        return log10_energy_sum;
    };

    auto calc_delta = [&]() -> double {
        T lb = get_lower_bound_log10();
        T ub = get_upper_bound_log10();
        if (ub == pos_inf || ub <= neg_inf || lb <= neg_inf) {
            return 1.0;
        }
        T diff = lb - ub;
        if (diff < T(-36)) {
            return 1.0;
        }
        return 1.0 - std::pow(10.0, static_cast<double>(diff));
    };

    auto has_low_energies = [&]() -> bool {
        // Java stops when minLowerScoreWeight <= 0 (underflow in BigDecimal precision).
        // In log-space, mimic this by stopping if the minimum score weight is effectively 0.
        if (log10_min_lower_score_w == pos_inf) {
            return true; // no energies yet
        }
        // If weight is so tiny it won't change any double-based result, allow early stop.
        return log10_min_lower_score_w > T(-350);
    };

    auto do_score_batch = [&](int numScores) -> bool {
        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();

        int got = 0;
        for (int i = 0; i < numScores; ++i) {
            auto leaf = next_scored_leaf();
            if (!leaf) {
                break;
            }
            if (std::isinf(leaf->score) && leaf->score > T(0)) {
                break;
            }

            // compute weight for score
            T log10_w = log10BoltzmannWeight(leaf->score);
            if (log10_upper_score_sum == neg_inf) {
                log10_upper_score_sum = log10_w;
            } else {
                log10_upper_score_sum = log10Add(log10_upper_score_sum, log10_w);
            }
            log10_min_upper_score_w = std::min(log10_min_upper_score_w, log10_w);

            buf.push_back(*std::move(leaf));
            ++num_scored;
            ++got;
        }

        auto t1 = clock::now();
        std::chrono::duration<double> dt = t1 - t0;
        if (dt.count() > 0.0 && got > 0) {
            scoreOps = static_cast<double>(got) / dt.count();
        }

        // update slope like Java onScores
        double delta = calc_delta();
        dScore = calcSlope(delta, prevDelta, dEnergy);
        prevDelta = delta;
        dEnergy *= 2.0;

        return got > 0;
    };

    auto do_energy_one = [&]() -> bool {
        if (buf.empty()) {
            return false;
        }

        using clock = std::chrono::steady_clock;
        auto t0 = clock::now();

        ScoredLeaf leaf = std::move(buf.front());
        buf.pop_front();

        // scoreWeight and energyWeight
        // In this C++ Phase-1 port, score==energy (no minimization).
        T score = leaf.score;
        T energy = astar.computeGScore(leaf.node);

        T log10_score_w = log10BoltzmannWeight(score);
        T log10_energy_w = log10BoltzmannWeight(energy);

        if (log10_lower_score_sum == neg_inf) {
            log10_lower_score_sum = log10_score_w;
        } else {
            log10_lower_score_sum = log10Add(log10_lower_score_sum, log10_score_w);
        }
        if (log10_energy_sum == neg_inf) {
            log10_energy_sum = log10_energy_w;
        } else {
            log10_energy_sum = log10Add(log10_energy_sum, log10_energy_w);
        }

        log10_min_lower_score_w = std::min(log10_min_lower_score_w, log10_score_w);
        ++num_energied;

        auto t1 = clock::now();
        std::chrono::duration<double> dt = t1 - t0;
        if (dt.count() > 0.0) {
            energyOps = 1.0 / dt.count();
        }

        // update slope like Java onEnergy
        double delta = calc_delta();
        dEnergy = calcSlope(delta, prevDelta, dScore);
        prevDelta = delta;
        dScore *= 2.0;

        return true;
    };

    // Java GD does scoring first to escape initial flat spot.
    // Do at least one scored conf before any energy step.
    if (!do_score_batch(10)) {
        // no conformations
        result.lower_bound = neg_inf;
        result.upper_bound = neg_inf;
        result.delta = T(1);
        result.converged = false;
        result.num_confs = 0;
        return result;
    }

    // Main loop: run until epsilon reached or out-of-conformations / out-of-low-energies
    while (true) {
        if (std::isnan(dEnergy) || std::isnan(dScore)) {
            break;
        }
        if (calc_delta() <= static_cast<double>(epsilon)) {
            break;
        }
        if (!has_low_energies()) {
            break;
        }

        bool scoreAheadOfEnergy = (num_energied < num_scored);
        bool energySteeperThanScore = (dEnergy <= dScore);

        enum class Step { None, Score, Energy };
        Step step = Step::None;
        int numScores = 0;

        // Choose which step to take (ported from Java logic)
        if (!buf.empty() && ((scoreAheadOfEnergy && energySteeperThanScore) || open_set.empty())) {
            step = Step::Energy;
        } else {
            step = Step::Score;
            double scoringSeconds = std::max(0.1 / energyOps, 0.01);
            numScores = std::max(static_cast<int>(scoringSeconds * scoreOps), 10);
        }

        bool progressed = false;
        switch (step) {
            case Step::Energy:
                progressed = do_energy_one();
                break;
            case Step::Score:
                progressed = do_score_batch(numScores);
                break;
            case Step::None:
                progressed = false;
                break;
        }
        if (!progressed) {
            // Can't make progress: out of conformations and/or buffer empty
            break;
        }

        // Safety: always keep score ahead to satisfy the Splitter invariant
        if (buf.empty() && !open_set.empty()) {
            (void)do_score_batch(10);
        }
    }

    // If we never energied anything, still produce bounds from scores
    // (this can happen if epsilon is extremely loose)
    T log10_lb = (log10_energy_sum > neg_inf) ? log10_energy_sum : neg_inf;
    T log10_ub = get_upper_bound_log10();
    if (log10_ub == pos_inf && log10_upper_score_sum > neg_inf) {
        // fallback: UB ~= upperScoreSum (already log10)
        log10_ub = log10_upper_score_sum;
    }
    if (log10_ub <= neg_inf && log10_lb > neg_inf) {
        log10_ub = log10_lb;
    }

    result.lower_bound = log10_lb;
    result.upper_bound = log10_ub;
    result.num_confs = static_cast<int64_t>(num_energied);

    if (log10_ub > neg_inf && log10_lb > neg_inf) {
        T diff_log = log10_lb - log10_ub;
        if (diff_log < T(-36)) {
            result.delta = T(1);
        } else {
            result.delta = T(1) - std::pow(T(10), diff_log);
        }
        result.converged = (result.delta <= epsilon);
    } else {
        result.delta = T(1);
        result.converged = false;
    }

    return result;
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::compute(
    const EnergyMatrix<T>& emat,
    T epsilon,
    PartitionFunctionMethod method
) {
    return compute(emat, epsilon, method, ComputeOptions{});
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::compute(
    const EnergyMatrix<T>& emat,
    T epsilon,
    PartitionFunctionMethod method,
    ComputeOptions options
) {
    // Keep the "exact enumeration" shortcut for A* path only, and only when allowed.
    if (options.allow_exact_enumeration && method == PartitionFunctionMethod::AStar) {
        constexpr int64_t exact_enum_threshold = 5'000'000;

        int64_t total_confs = 1;
        const int32_t num_positions = emat.getNumPositions();
        for (int32_t pos = 0; pos < num_positions; ++pos) {
            total_confs *= static_cast<int64_t>(emat.getNumConfsAtPos(pos));
            if (total_confs > exact_enum_threshold) {
                break;
            }
        }

        if (total_confs <= exact_enum_threshold) {
            return computeExactByEnumeration(emat);
        }
    }

    switch (method) {
        case PartitionFunctionMethod::AStar:
            return computeWithAStar(emat, epsilon, options);
        case PartitionFunctionMethod::GradientDescent:
            return computeWithGradientDescent(emat, epsilon);
    }

    return computeWithAStar(emat, epsilon, options);
}

template<std::floating_point T>
[[nodiscard]] PartitionFunctionResult<T> PartitionFunction<T>::compute(
    const Sequence& /* seq */,
    const ConfSpace<T>& /* confspace */,
    T /* epsilon */
) {
    // TODO: Full implementation with ConfSpace
    // For now, this requires EnergyMatrix to be pre-computed
    // Placeholder implementation
    PartitionFunctionResult<T> result;
    result.lower_bound = T(0);
    result.upper_bound = T(0);
    result.delta = T(1);
    result.num_confs = 0;
    result.converged = false;
    
    return result;
}

// Explicit instantiation for common types
template class PartitionFunction<double>;
template class PartitionFunction<float>;

} // namespace kstar
} // namespace osprey

