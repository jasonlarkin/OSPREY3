#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "astar_node_fast.hpp"
#include "astar_search_fast.hpp"
#include "energy_matrix.hpp"
#include "log_space.hpp"
#include "min_heap.hpp"
#include "partition_function.hpp"

using namespace osprey::kstar;

namespace {

constexpr double kRT = 0.001987 * 298.15; // must match PartitionFunction<T>::RT
constexpr double kLn10 = 2.3025850929940459;

static double log10BoltzmannWeight(double energy) noexcept {
    if (std::isinf(energy)) {
        if (energy > 0.0) {
            return std::numeric_limits<double>::lowest();
        }
        return std::numeric_limits<double>::max();
    }
    if (std::isnan(energy)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    return -energy / (kRT * kLn10);
}

static int64_t totalConfs(const EnergyMatrix<double>& emat) {
    int64_t total = 1;
    const int32_t npos = emat.getNumPositions();
    for (int32_t pos = 0; pos < npos; ++pos) {
        total *= static_cast<int64_t>(emat.getNumConfsAtPos(pos));
    }
    return total;
}

static EnergyMatrix<double> makeTinyDeterministicEmat() {
    // Small enough to compute exact oracle, but non-trivial enough to have many leaves.
    // 4 positions, 3 RC each => 81 conformations.
    std::vector<int32_t> nrc = {3, 3, 3, 3};
    EnergyMatrix<double> emat(4, nrc);

    emat.setConstTerm(0.0);

    // One-body biases create a single clear best conformation, and many clearly-worse ones.
    for (int32_t pos = 0; pos < 4; ++pos) {
        for (int32_t rc = 0; rc < 3; ++rc) {
            // rc=0 is best, rc=2 is worst.
            emat.setOneBody(pos, rc, 0.25 * rc + 0.01 * pos);
        }
    }

    // Pairwise terms add mild interaction structure but stay small.
    for (int32_t pos1 = 1; pos1 < 4; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            for (int32_t rc1 = 0; rc1 < 3; ++rc1) {
                for (int32_t rc2 = 0; rc2 < 3; ++rc2) {
                    const double v = 0.005 * (rc1 - rc2);
                    emat.setPairwise(pos1, rc1, pos2, rc2, v);
                }
            }
        }
    }

    return emat;
}

struct PrunedResult {
    double log10_q = std::numeric_limits<double>::lowest();
    int64_t leaves = 0;
    double best_energy = std::numeric_limits<double>::max();
};

static PrunedResult computePrunedAStarQ_Log10(const EnergyMatrix<double>& emat) {
    // This is intentionally WRONG for partition functions:
    // it applies GMEC-style pruning (discard nodes whose lower bound exceeds the best leaf energy seen so far).
    //
    // This demonstrates why PartitionFunction explicitly forbids pruning: it underestimates Q.
    const int32_t npos = emat.getNumPositions();
    std::vector<int32_t> nrc(static_cast<std::size_t>(npos));
    for (int32_t pos = 0; pos < npos; ++pos) {
        nrc[static_cast<std::size_t>(pos)] = emat.getNumConfsAtPos(pos);
    }

    AStarSearchFast<double> astar(emat, npos, nrc);

    struct NodeMinScore final {
        bool operator()(const AStarNodeFast<double>& a, const AStarNodeFast<double>& b) const noexcept {
            return a.getScore() > b.getScore();
        }
    };

    MinHeap<AStarNodeFast<double>, NodeMinScore> open(NodeMinScore{});

    {
        auto root = AStarNodeFast<double>::root(npos);
        root.g_score = astar.computeGScore(root);
        root.h_score = astar.computeHScore(root);
        root.f_score = root.g_score + root.h_score;
        open.push(std::move(root));
    }

    PrunedResult out;

    while (!open.empty()) {
        auto node = open.pop();

        if (astar.isLeaf(node)) {
            // At a leaf: g_score is the exact energy.
            const double e = static_cast<double>(node.g_score);
            out.best_energy = std::min(out.best_energy, e);
            const double w = log10BoltzmannWeight(e);
            out.log10_q = (out.log10_q == std::numeric_limits<double>::lowest())
                ? w
                : logspace::log10Add(out.log10_q, w);
            out.leaves++;
            continue;
        }

        auto children = astar.expand(node);
        for (auto& child : children) {
            // WRONG prune: once a best leaf exists, discard any node whose *lower bound* exceeds it.
            if (out.best_energy < std::numeric_limits<double>::max()) {
                const double lb = static_cast<double>(child.getScore());
                if (lb > out.best_energy) {
                    continue;
                }
            }
            open.push(std::move(child));
        }
    }

    return out;
}

} // namespace

TEST(PartitionFunction_Tutorial, PruningUnderestimatesQ_ComparedToExactOracle) {
    const auto emat = makeTinyDeterministicEmat();
    const int64_t total = totalConfs(emat);
    ASSERT_GT(total, 1);

    // Exact oracle via the enumeration shortcut (small space).
    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = true;
    const auto exact = pfunc.compute(emat, /*epsilon=*/1e-6, PartitionFunctionMethod::AStar, opts);
    ASSERT_TRUE(exact.converged);
    ASSERT_DOUBLE_EQ(exact.delta, 0.0);
    ASSERT_DOUBLE_EQ(exact.lower_bound, exact.upper_bound);
    ASSERT_EQ(exact.num_confs, total);

    // Intentionally wrong pruned run.
    const auto pruned = computePrunedAStarQ_Log10(emat);
    ASSERT_GT(pruned.leaves, 0);
    ASSERT_LT(pruned.leaves, total); // pruning should drop many conformations

    // Q is a sum of positive weights: pruning removes terms => underestimates Q => log10(Q) smaller.
    EXPECT_LT(pruned.log10_q, static_cast<double>(exact.lower_bound));
}

