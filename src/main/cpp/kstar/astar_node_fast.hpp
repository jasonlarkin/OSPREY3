#ifndef OSPREY_KSTAR_ASTAR_NODE_FAST_HPP
#define OSPREY_KSTAR_ASTAR_NODE_FAST_HPP

#include <array>
#include <vector>
#include <cstdint>
#include <concepts>

namespace osprey {
namespace kstar {

/**
 * "Fast" A* node variant.
 *
 * Goal: keep the baseline A* implementation intact for profiling/tracking, while
 * providing a faster node representation that avoids per-node heap allocations for
 * small position counts (common in our current benchmark cases).
 *
 * This file is intentionally separate from `astar_node.hpp` to preserve the baseline.
 */
template<std::floating_point T>
struct AStarNodeFast {
    // Assignment: position -> RC (or -1 if unassigned).
    // Inline storage for small num_positions, heap fallback otherwise.
    static constexpr int32_t kInlineCapacity = 16;
    std::array<int16_t, kInlineCapacity> inline_assignments{};
    std::vector<int16_t> heap_assignments;
    int32_t num_positions = 0;
    bool uses_heap = false;

    // Energy bounds
    T g_score;  // Lower bound: actual energy of assigned positions
    T h_score;  // Upper bound heuristic: optimistic estimate for unassigned positions

    // Tree depth (number of assigned positions)
    int32_t level;

    [[nodiscard]] T getScore() const noexcept {
        return g_score + h_score;
    }

    /**
     * Create root node (all positions unassigned).
     */
    static AStarNodeFast root(int32_t num_positions_in) {
        AStarNodeFast node;
        node.num_positions = num_positions_in;
        node.uses_heap = (num_positions_in > kInlineCapacity);
        if (node.uses_heap) {
            node.heap_assignments.assign(static_cast<size_t>(num_positions_in), -1);
        } else {
            node.inline_assignments.fill(-1);
        }
        node.g_score = T(0);
        node.h_score = T(0);
        node.level = 0;
        return node;
    }

    /**
     * Create child node by assigning a position.
     */
    [[nodiscard]] AStarNodeFast assign(int32_t pos, int16_t rc) const {
        AStarNodeFast child = *this;
        child.data()[pos] = rc;
        child.level = level + 1;
        return child;
    }

    [[nodiscard]] const int16_t* data() const noexcept {
        return uses_heap ? heap_assignments.data() : inline_assignments.data();
    }

    [[nodiscard]] int16_t* data() noexcept {
        return uses_heap ? heap_assignments.data() : inline_assignments.data();
    }

    [[nodiscard]] int32_t size() const noexcept {
        return num_positions;
    }

    /**
     * Check if position is assigned.
     */
    [[nodiscard]] bool isAssigned(int32_t pos) const noexcept {
        return data()[pos] >= 0;
    }

    /**
     * Get RC at position (or -1 if unassigned).
     */
    [[nodiscard]] int16_t getRC(int32_t pos) const noexcept {
        return data()[pos];
    }
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ASTAR_NODE_FAST_HPP


