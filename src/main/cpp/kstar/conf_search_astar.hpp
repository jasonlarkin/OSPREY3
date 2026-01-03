#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <queue>
#include <span>
#include <utility>
#include <vector>

#include "astar_node.hpp"
#include "astar_node_fast.hpp"
#include "astar_search.hpp"
#include "astar_search_fast.hpp"
#include "conf_search.hpp"
#include "energy_matrix.hpp"
#include "partition_function.hpp" // for AStarVariant

namespace osprey::kstar {

// ConfSearch implementation backed by the C++ A* search (baseline or fast).
//
// Semantics:
// - Enumerates conformations in non-decreasing A* score (f = g + h).
// - For leaf nodes, h==0, so score==energy for Phase-1 EnergyMatrix-based searches.
// - Returns assignments as int32_t RC IDs per position.
//
// Note: this is intentionally EnergyMatrix-only (Phase-1). A ConfSpace/JNA-backed search
// can implement the same ConfSearch interface later.

template<typename NodeT>
struct AStarNodeMinScore final {
    bool operator()(const NodeT& a, const NodeT& b) const noexcept {
        return a.getScore() > b.getScore();
    }
};

template<std::floating_point T>
class AStarConfSearchBaseline final : public ConfSearch {
public:
    explicit AStarConfSearchBaseline(const EnergyMatrix<T>& emat);

    std::optional<ScoredConf> nextConf() override;
    std::uint64_t getNumConformations() const override { return total_confs_; }

private:
    const EnergyMatrix<T>& emat_;
    int32_t num_positions_ = 0;
    std::vector<int32_t> num_confs_per_pos_;
    std::uint64_t total_confs_ = 0;

    // Constructed after num_confs_per_pos_ is populated (AStarSearch copies the span).
    std::unique_ptr<AStarSearch<T>> astar_;
    std::priority_queue<AStarNode<T>, std::vector<AStarNode<T>>, AStarNodeMinScore<AStarNode<T>>> open_set_;

    void initOpenSet();
};

template<std::floating_point T>
class AStarConfSearchFast final : public ConfSearch {
public:
    explicit AStarConfSearchFast(const EnergyMatrix<T>& emat);

    std::optional<ScoredConf> nextConf() override;
    std::uint64_t getNumConformations() const override { return total_confs_; }

private:
    const EnergyMatrix<T>& emat_;
    int32_t num_positions_ = 0;
    std::vector<int32_t> num_confs_per_pos_;
    std::uint64_t total_confs_ = 0;

    // Constructed after num_confs_per_pos_ is populated (AStarSearchFast copies the span).
    std::unique_ptr<AStarSearchFast<T>> astar_;
    std::priority_queue<AStarNodeFast<T>, std::vector<AStarNodeFast<T>>, AStarNodeMinScore<AStarNodeFast<T>>> open_set_;

    void initOpenSet();
};

template<std::floating_point T>
[[nodiscard]] inline std::unique_ptr<ConfSearch> makeAStarConfSearch(
    const EnergyMatrix<T>& emat,
    AStarVariant variant
) {
    if (variant == AStarVariant::Fast) {
        return std::make_unique<AStarConfSearchFast<T>>(emat);
    }
    return std::make_unique<AStarConfSearchBaseline<T>>(emat);
}

} // namespace osprey::kstar

