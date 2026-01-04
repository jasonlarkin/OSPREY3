#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdint>
#include <exception>
#include <optional>
#include <string>
#include <vector>
#include <cstdlib>

#include "astar_search.hpp"
#include "astar_search_fast.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "min_heap.hpp"
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
    // Larger “production-ish” matrices already present in build test_data (use scripts/inspect_emat_bin.py to confirm).
    {"2RL0_Complex_Large", "test_data/2RL0.complex.emat.bin"},
    {"1CC8_ConfRanker_Huge", "test_data/1CC8.TestConfRanker.huge.emat.bin"},
    // True “production” benchmark hook: set OSPREY_KSTAR_ASTAR_BENCH_EMAT to an absolute path
    // for a Java-exported *.emat.bin (not committed).
    {"External", nullptr},
};

static std::optional<EnergyMatrix<double>> loadEmat(const char* rel) {
    try {
        if (rel == nullptr) {
            const char* p = std::getenv("OSPREY_KSTAR_ASTAR_BENCH_EMAT");
            if (p == nullptr || *p == '\0') {
                return std::nullopt;
            }
            return EnergyMatrixLoader<double>::loadFromFile(p);
        }
        auto p = osprey::kstar::testutil::resolveTestDataPath(rel);
        if (!p) {
            return std::nullopt;
        }
        return EnergyMatrixLoader<double>::loadFromFile(p->string());
    } catch (const std::exception&) {
        // Benchmarks should be resilient to missing external data (e.g., placeholder env var).
        // Returning nullopt lets the benchmark skip with a clear message instead of aborting.
        return std::nullopt;
    }
}

template<typename NodeT>
struct NodeMinScore final {
    bool operator()(const NodeT& a, const NodeT& b) const noexcept {
        return a.getScore() > b.getScore();
    }
};

template<typename NodeT>
using OpenSet = osprey::kstar::MinHeap<NodeT, NodeMinScore<NodeT>>;

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

    // Indirect heap: store only {score, index} in the heap so heap maintenance moves a tiny
    // object instead of copying/moving full NodeT values.
    struct HeapItem final {
        double score = 0.0;
        std::uint32_t idx = 0;
    };
    struct HeapItemMinScore final {
        bool operator()(const HeapItem& a, const HeapItem& b) const noexcept {
            return a.score > b.score;
        }
    };
    using HeapT = osprey::kstar::MinHeap<HeapItem, HeapItemMinScore>;

    std::vector<NodeT> pool;
    {
        // Heuristic reserve: in typical A* workloads, nodes generated is O(max_pops * avg_branching).
        // Use a modest multiplier and cap to keep memory reasonable.
        const std::size_t pool_reserve = static_cast<std::size_t>(std::min<std::int64_t>(max_pops * 64, 2'000'000));
        pool.reserve(pool_reserve);
    }

    HeapT open_set(HeapItemMinScore{});
    open_set.reserve(static_cast<std::size_t>(std::min<std::int64_t>(max_pops * 32, 1'000'000)));

    {
        auto root = NodeT::root(num_positions);
        root.g_score = search.computeGScore(root);
        root.h_score = search.computeHScore(root);
        if constexpr (requires { root.f_score; }) {
            root.f_score = root.g_score + root.h_score;
        }
        const double score = static_cast<double>(root.getScore());
        pool.push_back(std::move(root));
        open_set.push(HeapItem{score, 0});
    }

    SearchStats s;
    std::vector<NodeT> children_scratch;
    {
        int32_t max_rc = 0;
        for (int32_t pos = 0; pos < num_positions; ++pos) {
            max_rc = std::max(max_rc, num_confs_per_pos[pos]);
        }
        children_scratch.reserve(static_cast<std::size_t>(max_rc));
    }
    while (!open_set.empty() && s.pops < max_pops) {
        const auto item = open_set.pop();
        ++s.pops;

        const NodeT& node = pool[static_cast<std::size_t>(item.idx)];
        if (search.isLeaf(node)) {
            ++s.leaves;
            continue;
        }

        if constexpr (requires { search.expandInto(node, children_scratch); }) {
            search.expandInto(node, children_scratch);
            s.expands += 1;
            for (auto& child : children_scratch) {
                const double score = static_cast<double>(child.getScore());
                const std::uint32_t idx = static_cast<std::uint32_t>(pool.size());
                pool.push_back(std::move(child));
                open_set.push(HeapItem{score, idx});
            }
        } else {
            auto children = search.expand(node);
            s.expands += 1;
            for (auto& child : children) {
                const double score = static_cast<double>(child.getScore());
                const std::uint32_t idx = static_cast<std::uint32_t>(pool.size());
                pool.push_back(std::move(child));
                open_set.push(HeapItem{score, idx});
            }
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
        // google/benchmark deprecated DoNotOptimize(const&) for small trivially-copyable types.
        // Passing a prvalue selects the non-deprecated overload.
        benchmark::DoNotOptimize(static_cast<decltype(s.pops)>(s.pops));
        benchmark::DoNotOptimize(static_cast<decltype(s.expands)>(s.expands));
        benchmark::DoNotOptimize(static_cast<decltype(s.leaves)>(s.leaves));
        benchmark::DoNotOptimize(static_cast<decltype(s.max_open)>(s.max_open));
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
        // google/benchmark deprecated DoNotOptimize(const&) for small trivially-copyable types.
        // Passing a prvalue selects the non-deprecated overload.
        benchmark::DoNotOptimize(static_cast<decltype(s.pops)>(s.pops));
        benchmark::DoNotOptimize(static_cast<decltype(s.expands)>(s.expands));
        benchmark::DoNotOptimize(static_cast<decltype(s.leaves)>(s.leaves));
        benchmark::DoNotOptimize(static_cast<decltype(s.max_open)>(s.max_open));
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
    ->Args({3, 1000})
    ->Args({4, 1000})
    ->Args({5, 1000})
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK(BM_AStarFast_PopExpand)
    ->ArgNames({"case", "max_pops"})
    ->Args({0, 1000})
    ->Args({1, 1000})
    ->Args({2, 1000})
    ->Args({3, 1000})
    ->Args({4, 1000})
    ->Args({5, 1000})
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK_MAIN();

