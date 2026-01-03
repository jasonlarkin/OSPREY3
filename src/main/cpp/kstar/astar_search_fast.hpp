#ifndef OSPREY_KSTAR_ASTAR_SEARCH_FAST_HPP
#define OSPREY_KSTAR_ASTAR_SEARCH_FAST_HPP

#include <vector>
#include <optional>
#include <span>
#include <cstdint>
#include <concepts>

#include "astar_node_fast.hpp"
#include "energy_matrix.hpp"

namespace osprey {
namespace kstar {

/**
 * "Fast" A* search variant.
 *
 * Baseline A* (`AStarSearch`) intentionally allocates intermediate vectors/tables
 * during g/h computation. This variant uses the same search semantics but with a
 * tighter inner loop and a node representation optimized for small position counts.
 *
 * NOTE: This is meant for performance benchmarking/demos; correctness is ensured
 * by running shared unit tests against both variants.
 */
template<std::floating_point T>
class AStarSearchFast {
public:
    AStarSearchFast(
        const EnergyMatrix<T>& emat,
        int32_t num_positions,
        std::span<const int32_t> num_confs_per_pos
    );

    [[nodiscard]] T computeGScore(const AStarNodeFast<T>& node) const;
    [[nodiscard]] T computeHScore(const AStarNodeFast<T>& node) const;

    [[nodiscard]] std::vector<AStarNodeFast<T>> expand(const AStarNodeFast<T>& node) const;
    [[nodiscard]] bool isLeaf(const AStarNodeFast<T>& node) const;
    [[nodiscard]] std::optional<int32_t> getNextPosition(const AStarNodeFast<T>& node) const;

private:
    const EnergyMatrix<T>& emat_;
    int32_t num_positions_;
    std::vector<int32_t> num_confs_per_pos_;

    // undefined_energies_[pos1][rc1][pos2] for pos2 < pos1
    std::vector<std::vector<std::vector<T>>> undefined_energies_;

    void precomputeUndefinedEnergies();
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ASTAR_SEARCH_FAST_HPP


