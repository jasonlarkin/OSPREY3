#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "astar_search.hpp"
#include "astar_search_fast.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

namespace {

struct Case {
    const char* name;
    const char* emat_rel_path;
};

static constexpr Case kCases[] = {
    {"2RL0_Protein", "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin"},
    {"2RL0_Ligand",  "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin"},
    {"2RL0_Complex", "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin"},
};

static std::optional<EnergyMatrix<double>> loadEmat(const char* rel) {
    auto p = osprey::kstar::testutil::resolveTestDataPath(rel);
    if (!p) {
        return std::nullopt;
    }
    return EnergyMatrixLoader<double>::loadFromFile(p->string());
}

template<typename NodeT>
struct NodeMinScore final {
    bool operator()(const NodeT& a, const NodeT& b) const noexcept {
        return a.getScore() > b.getScore();
    }
};

template<typename NodeT>
using OpenSet = std::priority_queue<NodeT, std::vector<NodeT>, NodeMinScore<NodeT>>;

struct SearchStats final {
    std::int64_t pops = 0;
    std::int64_t expands = 0;
    std::int64_t leaves = 0;
    std::int64_t max_open = 0;
};

template<typename NodeT, typename SearchT>
static SearchStats runOneSearch(const EnergyMatrix<double>& emat, std::int64_t max_pops) {
    const int32_t num_positions = emat.getNumPositions();
    std::vector<int32_t> num_confs_per_pos(num_positions);
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        num_confs_per_pos[pos] = emat.getNumConfsAtPos(pos);
    }

    SearchT search(emat, num_positions, num_confs_per_pos);

    OpenSet<NodeT> open_set;
    {
        auto root = NodeT::root(num_positions);
        root.g_score = search.computeGScore(root);
        root.h_score = search.computeHScore(root);
        open_set.push(root);
    }

    SearchStats s;
    while (!open_set.empty() && s.pops < max_pops) {
        NodeT node = open_set.top();
        open_set.pop();
        ++s.pops;

        if (search.isLeaf(node)) {
            ++s.leaves;
            continue;
        }

        auto children = search.expand(node);
        s.expands += 1;
        for (const auto& child : children) {
            open_set.push(child);
        }
        s.max_open = std::max(s.max_open, static_cast<std::int64_t>(open_set.size()));
    }

    return s;
}

static void BM_AStarBaseline_PopExpand(benchmark::State& state) {
    const int idx = static_cast<int>(state.range(0));
    const std::int64_t max_pops = static_cast<std::int64_t>(state.range(1));

    auto ematOpt = loadEmat(kCases[idx].emat_rel_path);
    if (!ematOpt) {
        state.SkipWithError(
            (std::string("Missing test data: ") + kCases[idx].emat_rel_path + "\n" +
             osprey::kstar::testutil::describeTestDataSearch(kCases[idx].emat_rel_path))
                .c_str()
        );
        return;
    }

    using NodeT = AStarNode<double>;
    using SearchT = AStarSearch<double>;

    for (auto _ : state) {
        const auto s = runOneSearch<NodeT, SearchT>(*ematOpt, max_pops);
        benchmark::DoNotOptimize(s.pops);
        benchmark::DoNotOptimize(s.expands);
        benchmark::DoNotOptimize(s.leaves);
        benchmark::DoNotOptimize(s.max_open);
        benchmark::ClobberMemory();
    }

    const auto once = runOneSearch<NodeT, SearchT>(*ematOpt, max_pops);
    state.counters["num_pos"] = static_cast<double>((*ematOpt).getNumPositions());
    state.counters["max_pops"] = static_cast<double>(max_pops);
    state.counters["pops"] = static_cast<double>(once.pops);
    state.counters["expands"] = static_cast<double>(once.expands);
    state.counters["leaves"] = static_cast<double>(once.leaves);
    state.counters["max_open"] = static_cast<double>(once.max_open);
}

static void BM_AStarFast_PopExpand(benchmark::State& state) {
    const int idx = static_cast<int>(state.range(0));
    const std::int64_t max_pops = static_cast<std::int64_t>(state.range(1));

    auto ematOpt = loadEmat(kCases[idx].emat_rel_path);
    if (!ematOpt) {
        state.SkipWithError(
            (std::string("Missing test data: ") + kCases[idx].emat_rel_path + "\n" +
             osprey::kstar::testutil::describeTestDataSearch(kCases[idx].emat_rel_path))
                .c_str()
        );
        return;
    }

    using NodeT = AStarNodeFast<double>;
    using SearchT = AStarSearchFast<double>;

    for (auto _ : state) {
        const auto s = runOneSearch<NodeT, SearchT>(*ematOpt, max_pops);
        benchmark::DoNotOptimize(s.pops);
        benchmark::DoNotOptimize(s.expands);
        benchmark::DoNotOptimize(s.leaves);
        benchmark::DoNotOptimize(s.max_open);
        benchmark::ClobberMemory();
    }

    const auto once = runOneSearch<NodeT, SearchT>(*ematOpt, max_pops);
    state.counters["num_pos"] = static_cast<double>((*ematOpt).getNumPositions());
    state.counters["max_pops"] = static_cast<double>(max_pops);
    state.counters["pops"] = static_cast<double>(once.pops);
    state.counters["expands"] = static_cast<double>(once.expands);
    state.counters["leaves"] = static_cast<double>(once.leaves);
    state.counters["max_open"] = static_cast<double>(once.max_open);
}

} // namespace

BENCHMARK(BM_AStarBaseline_PopExpand)
    ->ArgNames({"case", "max_pops"})
    ->Args({0, 1000})
    ->Args({1, 1000})
    ->Args({2, 1000})
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK(BM_AStarFast_PopExpand)
    ->ArgNames({"case", "max_pops"})
    ->Args({0, 1000})
    ->Args({1, 1000})
    ->Args({2, 1000})
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK_MAIN();

