#include <benchmark/benchmark.h>

#include <optional>
#include <string>
#include <string_view>

#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "partition_function.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

namespace {

enum class Method {
    AStarBaseline,
    AStarFast,
    GradientDescent
};

struct Case {
    const char* name;
    const char* emat_rel_path;
    double epsilon;
};

static constexpr Case kCases[] = {
    {"2RL0_Protein", "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin", 0.05},
    {"2RL0_Ligand",  "test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin", 0.05},
    {"2RL0_Complex", "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin", 0.8},
    {"1GUA11_Protein","test_data/1GUA11.TestSimplePartitionFunction.protein.emat.bin", 0.9},
    {"1GUA11_Ligand", "test_data/1GUA11.TestSimplePartitionFunction.ligand.emat.bin", 0.9},
    {"1GUA11_Complex","test_data/1GUA11.TestSimplePartitionFunction.complex.emat.bin", 0.9},
};

static std::optional<EnergyMatrix<double>> loadEmat(const char* rel) {
    auto p = osprey::kstar::testutil::resolveTestDataPath(rel);
    if (!p) {
        return std::nullopt;
    }
    return EnergyMatrixLoader<double>::loadFromFile(p->string());
}

static void runPfuncBenchmark(benchmark::State& state, const Case& tc, Method method) {
    auto ematOpt = loadEmat(tc.emat_rel_path);
    if (!ematOpt) {
        state.SkipWithError(
            (std::string("Missing test data: ") + tc.emat_rel_path + "\n" +
             osprey::kstar::testutil::describeTestDataSearch(tc.emat_rel_path))
                .c_str()
        );
        return;
    }
    const auto emat = *std::move(ematOpt);

    PartitionFunction<double> pfunc;
    const auto pfMethod = (method == Method::GradientDescent) ? PartitionFunctionMethod::GradientDescent
                                                              : PartitionFunctionMethod::AStar;
    PartitionFunction<double>::ComputeOptions opts;
    // For benchmarking, we want search-vs-search comparisons. Disable the A* exact enumeration shortcut.
    opts.allow_exact_enumeration = false;
    if (method == Method::AStarFast) {
        opts.astar_variant = AStarVariant::Fast;
    } else {
        opts.astar_variant = AStarVariant::Baseline;
    }

    // Run the compute path inside the benchmark loop.
    for (auto _ : state) {
        auto result = pfunc.compute(emat, tc.epsilon, pfMethod, opts);
        benchmark::DoNotOptimize(result.lower_bound);
        benchmark::DoNotOptimize(result.upper_bound);
        benchmark::DoNotOptimize(result.delta);
        benchmark::DoNotOptimize(result.num_confs);
        benchmark::ClobberMemory();
    }

    // Basic counters for context (not a correctness check).
    // Note: for large systems, num_confs depends on epsilon convergence.
    {
        auto once = pfunc.compute(emat, tc.epsilon, pfMethod, opts);
        state.counters["num_pos"] = static_cast<double>(emat.getNumPositions());
        state.counters["num_confs_eval"] = static_cast<double>(once.num_confs);
        state.counters["delta"] = static_cast<double>(once.delta);
    }
}

static void BM_Pfunc_AStarBaseline(benchmark::State& state) {
    const int idx = static_cast<int>(state.range(0));
    runPfuncBenchmark(state, kCases[idx], Method::AStarBaseline);
}

static void BM_Pfunc_AStarFast(benchmark::State& state) {
    const int idx = static_cast<int>(state.range(0));
    runPfuncBenchmark(state, kCases[idx], Method::AStarFast);
}

static void BM_Pfunc_GD(benchmark::State& state) {
    const int idx = static_cast<int>(state.range(0));
    runPfuncBenchmark(state, kCases[idx], Method::GradientDescent);
}

} // namespace

BENCHMARK(BM_Pfunc_AStarBaseline)
    ->ArgNames({"case"})
    ->DenseRange(0, static_cast<int>(std::size(kCases) - 1))
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK(BM_Pfunc_AStarFast)
    ->ArgNames({"case"})
    ->DenseRange(0, static_cast<int>(std::size(kCases) - 1))
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK(BM_Pfunc_GD)
    ->ArgNames({"case"})
    ->DenseRange(0, static_cast<int>(std::size(kCases) - 1))
    ->UseRealTime()
    ->MinTime(0.5);

BENCHMARK_MAIN();


