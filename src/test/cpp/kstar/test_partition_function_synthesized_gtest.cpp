#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <optional>
#include <string>

#include "partition_function.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

static std::optional<EnergyMatrix<double>> tryLoadEmat(const std::string& emat_path) {
    auto resolved = osprey::kstar::testutil::resolveTestDataPath(emat_path);
    if (!resolved) {
        return std::nullopt;
    }
    std::ifstream file(resolved->string());
    if (!file.good()) {
        return std::nullopt;
    }
    file.close();
    return EnergyMatrixLoader<double>::loadFromFile(resolved->string());
}

// ===================== A* Variant Equivalence (Baseline vs Fast) =====================
//
// These are NOT OSPREY verbatim tests. They are C++-port guardrails to ensure that
// "Fast" A* implementation is behaviorally identical to the baseline A* search
// for the partition function computation.
static void assertAStarVariantsAgreeOrSkip(const char* emat_path, double epsilon, const char* label) {
    auto ematOpt = tryLoadEmat(emat_path);
    if (!ematOpt) {
        GTEST_SKIP() << "Energy matrix not found: " << emat_path << "\n"
                     << osprey::kstar::testutil::describeTestDataSearch(emat_path);
    }
    auto emat = *std::move(ematOpt);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    // Ensure actually exercise A* (not exact enumeration).
    opts.allow_exact_enumeration = false;

    opts.astar_variant = AStarVariant::Baseline;
    const auto base = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

    opts.astar_variant = AStarVariant::Fast;
    const auto fast = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

    SCOPED_TRACE(label);
    SCOPED_TRACE(::testing::Message() << "epsilon=" << epsilon
                                      << " base{lb=" << base.lower_bound << ", ub=" << base.upper_bound
                                      << ", delta=" << base.delta << ", converged=" << base.converged
                                      << ", num_confs=" << base.num_confs << "}"
                                      << " fast{lb=" << fast.lower_bound << ", ub=" << fast.upper_bound
                                      << ", delta=" << fast.delta << ", converged=" << fast.converged
                                      << ", num_confs=" << fast.num_confs << "}");

    EXPECT_NEAR(base.lower_bound, fast.lower_bound, 1e-10);
    EXPECT_NEAR(base.upper_bound, fast.upper_bound, 1e-10);
    EXPECT_NEAR(base.delta, fast.delta, 1e-12);
    EXPECT_EQ(base.converged, fast.converged);
    EXPECT_EQ(base.num_confs, fast.num_confs);
}

TEST(PartitionFunction_SYNTHESIZED_AStarVariants, TwoRL0_Protein) {
    assertAStarVariantsAgreeOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin",
        0.05,
        "2RL0 Protein"
    );
}

TEST(PartitionFunction_SYNTHESIZED_AStarVariants, TwoRL0_Complex) {
    assertAStarVariantsAgreeOrSkip(
        "test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin",
        0.8,
        "2RL0 Complex"
    );
}

TEST(PartitionFunction_SYNTHESIZED_AStarVariants, OneGUA11_Complex) {
    assertAStarVariantsAgreeOrSkip(
        "test_data/1GUA11.TestSimplePartitionFunction.complex.emat.bin",
        0.9,
        "1GUA11 Complex"
    );
}

