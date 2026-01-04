#ifndef OSPREY_KSTAR_ASTAR_SEARCH_FAST_HPP
#define OSPREY_KSTAR_ASTAR_SEARCH_FAST_HPP

#include <vector>
#include <optional>
#include <span>
#include <cstdint>
#include <concepts>
#include <cstddef>

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

    // Expand into caller-provided storage to avoid per-expand heap allocations in hot loops.
    // `out` will be cleared and filled with children of `node`.
    void expandInto(const AStarNodeFast<T>& node, std::vector<AStarNodeFast<T>>& out) const;

    [[nodiscard]] std::vector<AStarNodeFast<T>> expand(const AStarNodeFast<T>& node) const;
    [[nodiscard]] bool isLeaf(const AStarNodeFast<T>& node) const;
    [[nodiscard]] std::optional<int32_t> getNextPosition(const AStarNodeFast<T>& node) const;

private:
    const EnergyMatrix<T>& emat_;
    int32_t num_positions_;
    std::vector<int32_t> num_confs_per_pos_;
    std::vector<size_t> rc_offsets_;

    // undefined_energies_[pos1][rc1][pos2] for pos2 < pos1
    std::vector<std::vector<std::vector<T>>> undefined_energies_;
    // Prefix sums over pos2 for undefined_energies_ to allow O(1) range sums.
    // undefined_prefix_[pos1][rc1][t] = sum_{pos2 < t} undefined_energies_[pos1][rc1][pos2]
    // (t ranges 0..num_positions_).
    std::vector<std::vector<std::vector<T>>> undefined_prefix_;

    // Scratch buffers used by expand() to avoid per-call heap allocation.
    // Indexed via rc_offsets_[pos] + rc for per-position-per-rotamer storage.
    mutable std::vector<T> expand_base_buf_;
    // Scratch buffers sized to max #RC at any position, used to compute all children h-scores in bulk.
    mutable std::vector<T> expand_child_h_buf_;
    mutable std::vector<T> expand_min_for_rc_buf_;
    mutable std::vector<T> expand_child_g_buf_;

    void precomputeUndefinedEnergies();

    [[nodiscard]] T sumUndefinedRange(int32_t pos1, int32_t rc1, int32_t start_pos2, int32_t end_pos2_exclusive) const noexcept;
    // Hot-path fast-path used when callers already guarantee:
    // 0 <= start_pos2 <= end_pos2_exclusive <= pos1.
    [[nodiscard]] T sumUndefinedRangeUnchecked(int32_t pos1, int32_t rc1, int32_t start_pos2, int32_t end_pos2_exclusive) const noexcept;
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ASTAR_SEARCH_FAST_HPP


