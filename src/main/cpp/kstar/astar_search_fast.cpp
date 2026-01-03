#include "astar_search_fast.hpp"

#include <algorithm>
#include <limits>

namespace osprey {
namespace kstar {

template<std::floating_point T>
AStarSearchFast<T>::AStarSearchFast(
    const EnergyMatrix<T>& emat,
    int32_t num_positions,
    std::span<const int32_t> num_confs_per_pos
) : emat_(emat),
    num_positions_(num_positions),
    num_confs_per_pos_(num_confs_per_pos.begin(), num_confs_per_pos.end()) {
    precomputeUndefinedEnergies();
}

template<std::floating_point T>
void AStarSearchFast<T>::precomputeUndefinedEnergies() {
    undefined_energies_.resize(num_positions_);

    for (int32_t pos1 = 0; pos1 < num_positions_; ++pos1) {
        undefined_energies_[pos1].resize(num_confs_per_pos_[pos1]);

        for (int32_t rc1 = 0; rc1 < num_confs_per_pos_[pos1]; ++rc1) {
            undefined_energies_[pos1][rc1].resize(num_positions_, T(0));

            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                T min_energy = std::numeric_limits<T>::max();
                for (int32_t rc2 = 0; rc2 < num_confs_per_pos_[pos2]; ++rc2) {
                    // EnergyMatrix requires pos1 > pos2. This is satisfied because pos2 < pos1 here.
                    min_energy = std::min(min_energy, emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2));
                }
                undefined_energies_[pos1][rc1][pos2] = min_energy;
            }
        }
    }
}

template<std::floating_point T>
T AStarSearchFast<T>::computeGScore(const AStarNodeFast<T>& node) const {
    // Hot path: no allocations, directly iterate assignment array.
    T gscore = emat_.getConstTerm();
    const int16_t* a = node.data();

    // One-body terms
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        const int16_t rc = a[pos];
        if (rc >= 0) {
            gscore += emat_.getOneBodyUnchecked(pos, rc);
        }
    }

    // Pairwise terms for assigned pairs (pos1 > pos2)
    for (int32_t pos1 = 1; pos1 < num_positions_; ++pos1) {
        const int16_t rc1 = a[pos1];
        if (rc1 < 0) {
            continue;
        }
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            const int16_t rc2 = a[pos2];
            if (rc2 < 0) {
                continue;
            }
            gscore += emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2);
        }
    }

    return gscore;
}

template<std::floating_point T>
T AStarSearchFast<T>::computeHScore(const AStarNodeFast<T>& node) const {
    // Hot path: no allocations. Same heuristic semantics as baseline.
    const int16_t* a = node.data();

    T hscore = T(0);
    for (int32_t pos1 = 0; pos1 < num_positions_; ++pos1) {
        if (a[pos1] >= 0) {
            continue; // assigned
        }

        T min_energy = std::numeric_limits<T>::max();
        const int32_t nrc1 = num_confs_per_pos_[pos1];

        for (int32_t rc1 = 0; rc1 < nrc1; ++rc1) {
            T e = emat_.getOneBodyUnchecked(pos1, rc1);

            // pairwise with assigned positions
            // NOTE: with sequential expansion order (getNextPosition = first unassigned),
            // assigned positions are always < pos1, so pos1 > pos2 holds.
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                const int16_t rc2 = a[pos2];
                if (rc2 < 0) {
                    continue;
                }
                e += emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2);
            }

            // optimal pairwise with other unassigned positions (pos2 < pos1)
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                if (a[pos2] < 0) {
                    e += undefined_energies_[pos1][rc1][pos2];
                }
            }

            min_energy = std::min(min_energy, e);
        }

        hscore += min_energy;
    }

    return hscore;
}

template<std::floating_point T>
std::optional<int32_t> AStarSearchFast<T>::getNextPosition(const AStarNodeFast<T>& node) const {
    // Same semantics as baseline: first unassigned.
    const int16_t* a = node.data();
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        if (a[pos] < 0) {
            return pos;
        }
    }
    return std::nullopt;
}

template<std::floating_point T>
std::vector<AStarNodeFast<T>> AStarSearchFast<T>::expand(const AStarNodeFast<T>& node) const {
    std::vector<AStarNodeFast<T>> children;

    auto next_pos_opt = getNextPosition(node);
    if (!next_pos_opt.has_value()) {
        return children;
    }

    const int32_t next_pos = next_pos_opt.value();
    children.reserve(static_cast<size_t>(num_confs_per_pos_[next_pos]));

    for (int32_t rc = 0; rc < num_confs_per_pos_[next_pos]; ++rc) {
        auto child = node.assign(next_pos, static_cast<int16_t>(rc));
        child.g_score = computeGScore(child);
        child.h_score = computeHScore(child);
        children.push_back(child);
    }

    return children;
}

template<std::floating_point T>
bool AStarSearchFast<T>::isLeaf(const AStarNodeFast<T>& node) const {
    return node.level == num_positions_;
}

// Explicit instantiation
template class AStarSearchFast<double>;
template class AStarSearchFast<float>;

} // namespace kstar
} // namespace osprey


