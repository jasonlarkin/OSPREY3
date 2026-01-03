#ifndef OSPREY_KSTAR_ASTAR_NODE_HPP
#define OSPREY_KSTAR_ASTAR_NODE_HPP

#include <vector>
#include <cstdint>
#include <concepts>
#include <algorithm>

namespace osprey {
namespace kstar {

/**
 * A* search node for conformation space exploration.
 * 
 * Memory-efficient representation following OSPREY's LinkedConfAStarNode pattern:
 * - Stores assignments as position -> RC mapping (not full conformation copy)
 * - Uses int16_t for positions/RCs (memory optimization)
 * - Tracks g-score (lower bound) and h-score (upper bound heuristic)
 */
template<std::floating_point T>
struct AStarNode {
    // Assignment: position -> RC (or -1 if unassigned)
    // Using int16_t to match OSPREY's memory-efficient pattern
    std::vector<int16_t> assignments;
    
    // Energy bounds
    T g_score;  // Lower bound: actual energy of assigned positions
    T h_score;  // Upper bound heuristic: optimistic estimate for unassigned positions
    
    // Number of assigned positions (level in search tree)
    int32_t level;
    
    /**
     * Create root node (all positions unassigned).
     */
    static AStarNode root(int32_t num_positions) {
        AStarNode node;
        node.assignments.assign(num_positions, -1);
        node.g_score = T(0);
        node.h_score = T(0);
        node.level = 0;
        return node;
    }
    
    /**
     * Create child node by assigning a position.
     */
    [[nodiscard]] AStarNode assign(int32_t pos, int16_t rc) const {
        AStarNode child = *this;
        child.assignments[pos] = rc;
        child.level = level + 1;
        return child;
    }
    
    /**
     * Check if position is assigned.
     */
    [[nodiscard]] bool isAssigned(int32_t pos) const {
        return assignments[pos] >= 0;
    }
    
    /**
     * Get RC at position (or -1 if unassigned).
     */
    [[nodiscard]] int16_t getRC(int32_t pos) const {
        return assignments[pos];
    }
    
    /**
     * Get total score (f = g + h) for priority queue ordering.
     * Lower is better (min-heap).
     */
    [[nodiscard]] T getScore() const {
        return g_score + h_score;
    }
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ASTAR_NODE_HPP

