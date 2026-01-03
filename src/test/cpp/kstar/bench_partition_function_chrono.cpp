#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "partition_function.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

namespace {

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

static int parseIntArg(int argc, char** argv, std::string_view key, int def) {
    for (int i = 1; i < argc; ++i) {
        std::string_view s(argv[i]);
        if (s.rfind(key, 0) == 0) {
            std::string_view v = s.substr(key.size());
            if (!v.empty() && v[0] == '=') {
                v = v.substr(1);
            }
            return std::atoi(std::string(v).c_str());
        }
    }
    return def;
}

static void runOne(const Case& tc, PartitionFunctionMethod method, PartitionFunction<double>::ComputeOptions opts, int reps) {
    auto ematOpt = loadEmat(tc.emat_rel_path);
    if (!ematOpt) {
        std::cerr << "[SKIP] Missing test data: " << tc.emat_rel_path << "\n"
                  << osprey::kstar::testutil::describeTestDataSearch(tc.emat_rel_path);
        return;
    }
    const auto emat = *std::move(ematOpt);

    PartitionFunction<double> pfunc;
    PartitionFunctionResult<double> last{};
    // For benchmarking, disable A* exact enumeration so comparisons are meaningful.
    opts.allow_exact_enumeration = false;

    // Warm-up (once)
    last = pfunc.compute(emat, tc.epsilon, method, opts);

    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < reps; ++i) {
        last = pfunc.compute(emat, tc.epsilon, method, opts);
    }
    const auto t1 = std::chrono::steady_clock::now();

    const auto ms = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(t1 - t0).count();
    const double per_ms = ms / static_cast<double>(reps);

    const char* methodName = (method == PartitionFunctionMethod::AStar) ? ((opts.astar_variant == AStarVariant::Fast) ? "AStarFast" : "AStarBaseline")
                                                                        : "GD";
    std::cout << std::fixed << std::setprecision(3);
    std::cout << tc.name << " " << methodName
              << " reps=" << reps
              << " total_ms=" << ms
              << " per_ms=" << per_ms
              << " num_pos=" << emat.getNumPositions()
              << " num_confs_eval=" << last.num_confs
              << " delta=" << last.delta
              << "\n";
}

} // namespace

int main(int argc, char** argv) {
    const int reps = std::max(1, parseIntArg(argc, argv, "--reps", 1));

    std::cout << "kstar chrono benchmark (synthetic)\n";
    std::cout << "Hint: set OSPREY_KSTAR_TEST_DATA_DIR=<path-to-build/cpp/kstar/test_data> if needed.\n";

    for (const auto& tc : kCases) {
        {
            PartitionFunction<double>::ComputeOptions opts;
            opts.astar_variant = AStarVariant::Baseline;
            runOne(tc, PartitionFunctionMethod::AStar, opts, reps);
        }
        {
            PartitionFunction<double>::ComputeOptions opts;
            opts.astar_variant = AStarVariant::Fast;
            runOne(tc, PartitionFunctionMethod::AStar, opts, reps);
        }
        {
            PartitionFunction<double>::ComputeOptions opts;
            runOne(tc, PartitionFunctionMethod::GradientDescent, opts, reps);
        }
    }

    return 0;
}


