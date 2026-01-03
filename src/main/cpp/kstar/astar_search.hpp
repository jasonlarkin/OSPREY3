#ifndef OSPREY_KSTAR_ASTAR_SEARCH_HPP
#define OSPREY_KSTAR_ASTAR_SEARCH_HPP

#include <queue>
#include <vector>
#include <optional>
#include <span>
#include <cstdint>
#include <concepts>
#include "astar_node.hpp"
#include "energy_matrix.hpp"

namespace osprey {
namespace kstar {

// Forward declaration
template<std::floating_point T>
class ConfSpace;

/**
 * A* search tree for conformation space exploration.
 * 
 * Implements A* search following OSPREY's ConfAStarTree pattern:
 * - Uses priority queue for open set
 * - Computes g-score (lower bound) and h-score (upper bound) using EnergyMatrix
 * - Supports epsilon-approximation for partition function
 */
template<std::floating_point T>
class AStarSearch {
public:
    /**
     * Construct A* search tree.
     * 
     * @param emat Energy matrix (pre-computed pairwise energies)
     * @param num_positions Number of design positions
     * @param num_confs_per_pos Number of conformations per position (span for zero-copy)
     */
    AStarSearch(
        const EnergyMatrix<T>& emat,
        int32_t num_positions,
        std::span<const int32_t> num_confs_per_pos
    );
    
    /**
     * Compute g-score (lower bound) for a node.
     * 
     * G-score = constant term + one-body energies (assigned) + pairwise energies (assigned pairs)
     */
    [[nodiscard]] T computeGScore(const AStarNode<T>& node) const;
    
    /**
     * Compute h-score (upper bound heuristic) for a node.
     * 
     * H-score = sum over unassigned positions of minimum energy RC
     * (considering one-body + pairwise with assigned + optimal pairwise with unassigned)
     */
    [[nodiscard]] T computeHScore(const AStarNode<T>& node) const;
    
    /**
     * Expand a node: create child nodes for all possible assignments at next position.
     */
    [[nodiscard]] std::vector<AStarNode<T>> expand(const AStarNode<T>& node) const;
    
    /**
     * Check if node is a leaf (all positions assigned).
     */
    [[nodiscard]] bool isLeaf(const AStarNode<T>& node) const;
    
    /**
     * Get next unassigned position (for expansion order).
     * 
     * Returns std::nullopt if all positions are assigned.
     */
    [[nodiscard]] std::optional<int32_t> getNextPosition(const AStarNode<T>& node) const;
    
private:
    const EnergyMatrix<T>& emat_;
    int32_t num_positions_;
    std::vector<int32_t> num_confs_per_pos_;
    
    /**
     * Pre-compute optimal pairwise energies for h-score heuristic.
     * 
     * For each position pair (pos1, pos2) where pos1 > pos2:
     * - For each RC at pos1, find minimum pairwise energy over all RCs at pos2
     * - Store in undefined_energies_[pos1][rc1][pos2]
     */
    std::vector<std::vector<std::vector<T>>> undefined_energies_;
    
    void precomputeUndefinedEnergies();

    /**
     * Compute cached energies for h-score calculation.
     *
     * For each unassigned position and RC:
     * - One-body energy
     * - Pairwise with assigned positions
     * - Optimal pairwise with other unassigned positions
     */
    void computeCachedEnergies(
        const AStarNode<T>& node,
        std::vector<std::vector<T>>& cached_energies
    ) const;
};

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ASTAR_SEARCH_HPP

