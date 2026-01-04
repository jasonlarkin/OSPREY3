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
    rc_offsets_.assign(static_cast<size_t>(num_positions_) + 1, size_t(0));
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        rc_offsets_[static_cast<size_t>(pos) + 1] =
            rc_offsets_[static_cast<size_t>(pos)] + static_cast<size_t>(num_confs_per_pos_[pos]);
    }
    expand_base_buf_.assign(rc_offsets_[static_cast<size_t>(num_positions_)], T(0));
    const int32_t max_rc = *std::max_element(num_confs_per_pos_.begin(), num_confs_per_pos_.end());
    expand_child_h_buf_.assign(static_cast<size_t>(max_rc), T(0));
    expand_min_for_rc_buf_.assign(static_cast<size_t>(max_rc), T(0));
    expand_child_g_buf_.assign(static_cast<size_t>(max_rc), T(0));
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

    // Build prefix sums for fast range queries in computeHScore/expand.
    undefined_prefix_.resize(num_positions_);
    for (int32_t pos1 = 0; pos1 < num_positions_; ++pos1) {
        undefined_prefix_[pos1].resize(num_confs_per_pos_[pos1]);
        for (int32_t rc1 = 0; rc1 < num_confs_per_pos_[pos1]; ++rc1) {
            // prefix length num_positions_ + 1 so callers can use end_pos2_exclusive = pos1, etc.
            undefined_prefix_[pos1][rc1].assign(static_cast<size_t>(num_positions_) + 1, T(0));
            T run = T(0);
            for (int32_t pos2 = 0; pos2 < num_positions_; ++pos2) {
                if (pos2 < pos1) {
                    run += undefined_energies_[pos1][rc1][pos2];
                }
                undefined_prefix_[pos1][rc1][static_cast<size_t>(pos2) + 1] = run;
            }
        }
    }
}

template<std::floating_point T>
T AStarSearchFast<T>::sumUndefinedRange(int32_t pos1, int32_t rc1, int32_t start_pos2, int32_t end_pos2_exclusive) const noexcept {
    if (start_pos2 < 0) {
        start_pos2 = 0;
    }
    if (end_pos2_exclusive < start_pos2) {
        return T(0);
    }
    if (end_pos2_exclusive > num_positions_) {
        end_pos2_exclusive = num_positions_;
    }
    // undefined energies only defined for pos2 < pos1; clamp end to pos1.
    if (end_pos2_exclusive > pos1) {
        end_pos2_exclusive = pos1;
    }
    if (start_pos2 >= end_pos2_exclusive) {
        return T(0);
    }
    const auto& pref = undefined_prefix_[static_cast<size_t>(pos1)][static_cast<size_t>(rc1)];
    return pref[static_cast<size_t>(end_pos2_exclusive)] - pref[static_cast<size_t>(start_pos2)];
}

template<std::floating_point T>
T AStarSearchFast<T>::sumUndefinedRangeUnchecked(int32_t pos1, int32_t rc1, int32_t start_pos2, int32_t end_pos2_exclusive) const noexcept {
    const auto& pref = undefined_prefix_[static_cast<size_t>(pos1)][static_cast<size_t>(rc1)];
    return pref[static_cast<size_t>(end_pos2_exclusive)] - pref[static_cast<size_t>(start_pos2)];
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

    // With the sequential expansion order (always assign first unassigned),
    // assigned positions form a prefix [0, k), unassigned positions are [k, num_positions_).
    int32_t k = 0;
    for (; k < num_positions_; ++k) {
        if (a[k] < 0) {
            break;
        }
    }

    T hscore = T(0);
    for (int32_t pos1 = k; pos1 < num_positions_; ++pos1) {
        T min_energy = std::numeric_limits<T>::max();
        const int32_t nrc1 = num_confs_per_pos_[pos1];

        for (int32_t rc1 = 0; rc1 < nrc1; ++rc1) {
            T e = emat_.getOneBodyUnchecked(pos1, rc1);

            // Pairwise with assigned positions [0, k).
            for (int32_t pos2 = 0; pos2 < k; ++pos2) {
                const int16_t rc2 = a[pos2]; // assigned by construction
                e += emat_.getPairwiseAssumingPos1Greater(pos1, rc1, pos2, rc2);
            }

            // Optimal pairwise with other unassigned positions in [k, pos1).
            // Here 0 <= k <= pos1 by construction of the loop and the prefix [0,k) assignment contract.
            e += sumUndefinedRangeUnchecked(pos1, rc1, k, pos1);

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
void AStarSearchFast<T>::expandInto(const AStarNodeFast<T>& node, std::vector<AStarNodeFast<T>>& out) const {
    out.clear();

    // Under A* expansion contract ("always assign first unassigned"), the next position
    // is exactly the current depth/level.
    const int32_t next_pos = node.level;
    if (next_pos >= num_positions_) {
        return;
    }
    out.reserve(static_cast<size_t>(num_confs_per_pos_[next_pos]));

    // Optimization: expanding assigns next_pos for multiple RCs.
    // Avoid recomputing full g/h from scratch for each child.
    const int16_t* a = node.data();
    const int32_t k = next_pos;
    const int32_t nrc_k = num_confs_per_pos_[k];

    // Compute all child h-scores in one pass across positions:
    // child_h[rc_at_k] = sum_{pos1 > k} min_{rc1 at pos1} (base(pos1,rc1) + pairwise(pos1,rc1,k,rc_at_k))
    T* child_h = expand_child_h_buf_.data();
    for (int32_t rc = 0; rc < nrc_k; ++rc) {
        child_h[rc] = T(0);
    }

    // Precompute per-pos1 per-rc1 base energies for the post-assignment unassigned set [k+1, pos1).
    // This is shared across all children (independent of the chosen rc at k).
    // Use a reusable flat buffer to avoid per-expand allocations.
    for (int32_t pos1 = k + 1; pos1 < num_positions_; ++pos1) {
        const size_t base_off = rc_offsets_[static_cast<size_t>(pos1)];
        for (int32_t rc1 = 0; rc1 < num_confs_per_pos_[pos1]; ++rc1) {
            T e = emat_.getOneBodyUnchecked(pos1, rc1);
            // unassigned positions after assigning k are [k+1, pos1)
            e += sumUndefinedRangeUnchecked(pos1, rc1, k + 1, pos1);
            expand_base_buf_[base_off + static_cast<size_t>(rc1)] = e;
        }
        // Add pairwise with assigned positions [0,k) in a cache-friendly way without repeated accessor calls.
        for (int32_t pos2 = 0; pos2 < k; ++pos2) {
            const int16_t rc2 = a[pos2];
            const auto blk = emat_.getPairwiseBlockAssumingPos1Greater(pos1, pos2);
            const T* p = blk.data + rc2;
            for (int32_t rc1 = 0; rc1 < blk.n1; ++rc1) {
                expand_base_buf_[base_off + static_cast<size_t>(rc1)] += *p;
                p += blk.n2;
            }
        }
    }

    // Precompute parent g-score once (node.g_score is already set in the code paths).
    // Root nodes created via AStarNodeFast::root() start with g_score=0, but the true g-score
    // includes the EnergyMatrix const term. Ensure expand() is safe/correct even if callers
    // didn't pre-initialize g_score/h_score on the root.
    const T parent_g = (node.level == 0) ? emat_.getConstTerm() : node.g_score;

    // Accumulate child_h[] using a block view for the (pos1, k) interaction.
    // This reduces per-element call overhead and improves locality.
    T* min_for_rc = expand_min_for_rc_buf_.data();
    for (int32_t pos1 = k + 1; pos1 < num_positions_; ++pos1) {
        for (int32_t rc = 0; rc < nrc_k; ++rc) {
            min_for_rc[rc] = std::numeric_limits<T>::max();
        }
        const size_t base_off = rc_offsets_[static_cast<size_t>(pos1)];
        const auto blk_pk = emat_.getPairwiseBlockAssumingPos1Greater(pos1, k);
        for (int32_t rc1 = 0; rc1 < num_confs_per_pos_[pos1]; ++rc1) {
            const T base_e = expand_base_buf_[base_off + static_cast<size_t>(rc1)];
            const T* row = blk_pk.data + static_cast<size_t>(rc1) * static_cast<size_t>(blk_pk.n2); // length nrc_k
            for (int32_t rc = 0; rc < nrc_k; ++rc) {
                const T e = base_e + row[static_cast<size_t>(rc)];
                if (e < min_for_rc[rc]) {
                    min_for_rc[rc] = e;
                }
            }
        }
        for (int32_t rc = 0; rc < nrc_k; ++rc) {
            child_h[rc] += min_for_rc[rc];
        }
    }

    // Compute g-score for all children in bulk (avoid repeating pairwise accessor calls).
    T* child_g = expand_child_g_buf_.data();
    for (int32_t rc = 0; rc < nrc_k; ++rc) {
        child_g[rc] = parent_g + emat_.getOneBodyUnchecked(k, rc);
    }
    for (int32_t pos2 = 0; pos2 < k; ++pos2) {
        const int16_t rc2 = a[pos2];
        const auto blk = emat_.getPairwiseBlockAssumingPos1Greater(k, pos2);
        const T* p = blk.data + rc2;
        for (int32_t rc = 0; rc < nrc_k; ++rc) {
            child_g[rc] += *p;
            p += blk.n2;
        }
    }

    for (int32_t rc = 0; rc < nrc_k; ++rc) {
        auto child = node.assign(k, static_cast<int16_t>(rc));

        child.g_score = child_g[rc];
        child.h_score = child_h[rc];
        child.f_score = child.g_score + child.h_score;
        out.push_back(child);
    }
}

template<std::floating_point T>
std::vector<AStarNodeFast<T>> AStarSearchFast<T>::expand(const AStarNodeFast<T>& node) const {
    std::vector<AStarNodeFast<T>> children;
    expandInto(node, children);
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


