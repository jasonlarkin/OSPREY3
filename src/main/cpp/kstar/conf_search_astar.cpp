#include "conf_search_astar.hpp"

#include <limits>
#include <stdexcept>

namespace osprey::kstar {

static std::uint64_t computeTotalConfsOrThrow(const std::vector<int32_t>& num_confs_per_pos) {
    std::uint64_t total = 1;
    for (int32_t n : num_confs_per_pos) {
        if (n < 0) {
            throw std::invalid_argument("negative num_confs_per_pos");
        }
        const std::uint64_t m = static_cast<std::uint64_t>(n);
        if (m == 0) {
            return 0;
        }
        if (total > std::numeric_limits<std::uint64_t>::max() / m) {
            throw std::overflow_error("total conformations overflow (uint64_t)");
        }
        total *= m;
    }
    return total;
}

template<std::floating_point T>
AStarConfSearchBaseline<T>::AStarConfSearchBaseline(const EnergyMatrix<T>& emat)
    : emat_(emat)
    , num_positions_(emat.getNumPositions())
    , num_confs_per_pos_(static_cast<size_t>(num_positions_))
{
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        num_confs_per_pos_[static_cast<size_t>(pos)] = emat.getNumConfsAtPos(pos);
    }
    total_confs_ = computeTotalConfsOrThrow(num_confs_per_pos_);
    astar_ = std::make_unique<AStarSearch<T>>(
        emat, num_positions_, std::span<const int32_t>(num_confs_per_pos_.data(), num_confs_per_pos_.size())
    );
    initOpenSet();
}

template<std::floating_point T>
void AStarConfSearchBaseline<T>::initOpenSet() {
    while (!open_set_.empty()) {
        open_set_.pop();
    }
    auto root = AStarNode<T>::root(num_positions_);
    root.g_score = astar_->computeGScore(root);
    root.h_score = astar_->computeHScore(root);
    open_set_.push(root);
}

template<std::floating_point T>
std::optional<ScoredConf> AStarConfSearchBaseline<T>::nextConf() {
    while (!open_set_.empty()) {
        AStarNode<T> node = open_set_.top();
        open_set_.pop();

        if (astar_->isLeaf(node)) {
            ScoredConf out;
            out.assignments.resize(static_cast<size_t>(num_positions_));
            for (int32_t pos = 0; pos < num_positions_; ++pos) {
                out.assignments[static_cast<size_t>(pos)] = static_cast<int32_t>(node.getRC(pos));
            }
            // At a leaf, h == 0, so score == energy for this Phase-1 port.
            out.score = static_cast<double>(node.getScore());
            return out;
        }

        auto children = astar_->expand(node);
        for (const auto& child : children) {
            open_set_.push(child);
        }
    }
    return std::nullopt;
}

template<std::floating_point T>
AStarConfSearchFast<T>::AStarConfSearchFast(const EnergyMatrix<T>& emat)
    : emat_(emat)
    , num_positions_(emat.getNumPositions())
    , num_confs_per_pos_(static_cast<size_t>(num_positions_))
{
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        num_confs_per_pos_[static_cast<size_t>(pos)] = emat.getNumConfsAtPos(pos);
    }
    total_confs_ = computeTotalConfsOrThrow(num_confs_per_pos_);
    astar_ = std::make_unique<AStarSearchFast<T>>(
        emat, num_positions_, std::span<const int32_t>(num_confs_per_pos_.data(), num_confs_per_pos_.size())
    );
    initOpenSet();
}

template<std::floating_point T>
void AStarConfSearchFast<T>::initOpenSet() {
    while (!open_set_.empty()) {
        open_set_.pop();
    }
    auto root = AStarNodeFast<T>::root(num_positions_);
    root.g_score = astar_->computeGScore(root);
    root.h_score = astar_->computeHScore(root);
    open_set_.push(root);
}

template<std::floating_point T>
std::optional<ScoredConf> AStarConfSearchFast<T>::nextConf() {
    while (!open_set_.empty()) {
        AStarNodeFast<T> node = open_set_.top();
        open_set_.pop();

        if (astar_->isLeaf(node)) {
            ScoredConf out;
            out.assignments.resize(static_cast<size_t>(num_positions_));
            const int16_t* a = node.data();
            for (int32_t pos = 0; pos < num_positions_; ++pos) {
                out.assignments[static_cast<size_t>(pos)] = static_cast<int32_t>(a[pos]);
            }
            out.score = static_cast<double>(node.getScore());
            return out;
        }

        auto children = astar_->expand(node);
        for (const auto& child : children) {
            open_set_.push(child);
        }
    }
    return std::nullopt;
}

template class AStarConfSearchBaseline<double>;
template class AStarConfSearchBaseline<float>;
template class AStarConfSearchFast<double>;
template class AStarConfSearchFast<float>;

} // namespace osprey::kstar

