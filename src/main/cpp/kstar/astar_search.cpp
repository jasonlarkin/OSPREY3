#include "astar_search.hpp"
#include <algorithm>
#include <limits>
#include <cmath>

namespace osprey {
namespace kstar {

template<std::floating_point T>
AStarSearch<T>::AStarSearch(
    const EnergyMatrix<T>& emat,
    int32_t num_positions,
    std::span<const int32_t> num_confs_per_pos
) : emat_(emat), num_positions_(num_positions), num_confs_per_pos_(num_confs_per_pos.begin(), num_confs_per_pos.end()) {
    precomputeUndefinedEnergies();
}

template<std::floating_point T>
void AStarSearch<T>::precomputeUndefinedEnergies() {
    // Pre-compute optimal pairwise energies for h-score heuristic
    // undefined_energies_[pos1][rc1][pos2] = min over rc2 of pairwise(pos1, rc1, pos2, rc2)
    
    undefined_energies_.resize(num_positions_);
    
    for (int32_t pos1 = 0; pos1 < num_positions_; ++pos1) {
        undefined_energies_[pos1].resize(num_confs_per_pos_[pos1]);
        
        for (int32_t rc1 = 0; rc1 < num_confs_per_pos_[pos1]; ++rc1) {
            undefined_energies_[pos1][rc1].resize(num_positions_, T(0));
            
            // For each position pos2 < pos1, find minimum pairwise energy
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                T min_energy = std::numeric_limits<T>::max();
                
                for (int32_t rc2 = 0; rc2 < num_confs_per_pos_[pos2]; ++rc2) {
                    // pos1 > pos2 by loop structure
                    T pairwise = emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2);
                    min_energy = std::min(min_energy, pairwise);
                }
                
                undefined_energies_[pos1][rc1][pos2] = min_energy;
            }
        }
    }
}

template<std::floating_point T>
T AStarSearch<T>::computeGScore(const AStarNode<T>& node) const {
    // G-score = constant term + one-body (assigned) + pairwise (assigned pairs)
    T gscore = emat_.getConstTerm();

    // Collect assigned positions
    std::vector<int32_t> assigned_positions;
    std::vector<int16_t> assigned_rcs;

    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        if (node.isAssigned(pos)) {
            assigned_positions.push_back(pos);
            assigned_rcs.push_back(node.getRC(pos));
        }
    }

    // Add one-body energies
    for (size_t i = 0; i < assigned_positions.size(); ++i) {
        int32_t pos = assigned_positions[i];
        int16_t rc = assigned_rcs[i];
        gscore += emat_.getOneBodyUnchecked(pos, rc);
    }

    // Add pairwise energies (only for pos1 > pos2 to avoid double counting)
    for (size_t i = 1; i < assigned_positions.size(); ++i) {
        int32_t pos1 = assigned_positions[i];
        int16_t rc1 = assigned_rcs[i];

        for (size_t j = 0; j < i; ++j) {
            int32_t pos2 = assigned_positions[j];
            int16_t rc2 = assigned_rcs[j];

            // assigned_positions is collected in ascending pos order, so pos1 > pos2 here
            gscore += emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2);
        }
    }
    
    return gscore;
}

template<std::floating_point T>
T AStarSearch<T>::computeHScore(const AStarNode<T>& node) const {
    // H-score = sum over unassigned positions of minimum energy RC

    // Collect unassigned positions
    std::vector<int32_t> unassigned_positions;
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        if (!node.isAssigned(pos)) {
            unassigned_positions.push_back(pos);
        }
    }

    if (unassigned_positions.empty()) {
        return T(0);  // All positions assigned, no heuristic needed
    }

    // Compute cached energies for unassigned positions
    std::vector<std::vector<T>> cached_energies(num_positions_);
    computeCachedEnergies(node, cached_energies);

    // Sum minimum energy over unassigned positions
    T hscore = T(0);
    for (int32_t pos : unassigned_positions) {
        T min_energy = std::numeric_limits<T>::max();

        for (int32_t rc = 0; rc < num_confs_per_pos_[pos]; ++rc) {
            min_energy = std::min(min_energy, cached_energies[pos][rc]);
        }

        hscore += min_energy;
    }

    return hscore;
}

template<std::floating_point T>
void AStarSearch<T>::computeCachedEnergies(
    const AStarNode<T>& node,
    std::vector<std::vector<T>>& cached_energies
) const {
    // Collect assigned and unassigned positions
    std::vector<int32_t> assigned_positions;
    std::vector<int16_t> assigned_rcs;
    std::vector<int32_t> unassigned_positions;

    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        if (node.isAssigned(pos)) {
            assigned_positions.push_back(pos);
            assigned_rcs.push_back(node.getRC(pos));
        } else {
            unassigned_positions.push_back(pos);
        }
    }

    // For each unassigned position and RC, compute cached energy
    for (int32_t pos1 : unassigned_positions) {
        cached_energies[pos1].resize(num_confs_per_pos_[pos1]);

        for (int32_t rc1 = 0; rc1 < num_confs_per_pos_[pos1]; ++rc1) {
            // Start with one-body energy
            T energy = emat_.getOneBodyUnchecked(pos1, rc1);

            // Add pairwise with assigned positions
            for (size_t i = 0; i < assigned_positions.size(); ++i) {
                int32_t pos2 = assigned_positions[i];
                int16_t rc2 = assigned_rcs[i];
                // With sequential expansion order (getNextPosition = first unassigned),
                // assigned positions are always < pos1, so pos1 > pos2 holds.
                energy += emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2);
            }

            // Add optimal pairwise with other unassigned positions
            for (int32_t pos2 : unassigned_positions) {
                if (pos2 < pos1) {
                    energy += undefined_energies_[pos1][rc1][pos2];
                }
            }

            cached_energies[pos1][rc1] = energy;
        }
    }
}

template<std::floating_point T>
std::optional<int32_t> AStarSearch<T>::getNextPosition(const AStarNode<T>& node) const {
    // Simple sequential ordering: return first unassigned position
    // TODO: Implement better ordering (e.g., entropy-based, score-based)
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        if (!node.isAssigned(pos)) {
            return pos;
        }
    }
    return std::nullopt;  // All positions assigned
}

template<std::floating_point T>
std::vector<AStarNode<T>> AStarSearch<T>::expand(const AStarNode<T>& node) const {
    std::vector<AStarNode<T>> children;
    
    auto next_pos_opt = getNextPosition(node);
    if (!next_pos_opt.has_value()) {
        return children;  // Leaf node, no expansion
    }
    
    int32_t next_pos = next_pos_opt.value();
    
    // Create child for each possible RC at next position
    for (int32_t rc = 0; rc < num_confs_per_pos_[next_pos]; ++rc) {
        AStarNode<T> child = node.assign(next_pos, static_cast<int16_t>(rc));
        
        // Compute bounds
        child.g_score = computeGScore(child);
        child.h_score = computeHScore(child);
        
        children.push_back(child);
    }
    
    return children;
}

template<std::floating_point T>
bool AStarSearch<T>::isLeaf(const AStarNode<T>& node) const {
    return node.level == num_positions_;
}

// Explicit instantiation
template class AStarSearch<double>;
template class AStarSearch<float>;

} // namespace kstar
} // namespace osprey

