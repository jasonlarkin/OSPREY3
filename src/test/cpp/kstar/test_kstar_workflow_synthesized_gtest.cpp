#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <string>

#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "kstar_workflow.hpp"
#include "partition_function.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

namespace {

static std::optional<EnergyMatrix<double>> loadEmat(const char* rel) {
    auto p = osprey::kstar::testutil::resolveTestDataPath(rel);
    if (!p) {
        return std::nullopt;
    }
    return EnergyMatrixLoader<double>::loadFromFile(p->string());
}

} // namespace

TEST(KStarWorkflow_SYNTHESIZED, ComputesLog10KStarFromThreeEnergyMatrices_2RL0) {
    auto protein = loadEmat("test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin");
    auto ligand = loadEmat("test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin");
    auto complex = loadEmat("test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin");

    if (!protein || !ligand || !complex) {
        GTEST_SKIP() << "Missing test data for 2RL0 emat triplet.\n"
                     << osprey::kstar::testutil::describeTestDataSearch("test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin");
    }

    constexpr double eps = 0.8;

    KStarWorkflow<double> wf;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false; // workflow comparisons should exercise search paths
    opts.astar_variant = AStarVariant::Baseline;

    const auto r = wf.compute(*protein, *ligand, *complex, eps, PartitionFunctionMethod::AStar, opts);

    EXPECT_LE(r.log10_lower_bound, r.log10_upper_bound);
    EXPECT_GE(r.log10_value, r.log10_lower_bound);
    EXPECT_LE(r.log10_value, r.log10_upper_bound);

    // Pfunc invariants are already tested elsewhere; just sanity check we got finite-ish outputs.
    EXPECT_FALSE(std::isnan(r.log10_lower_bound));
    EXPECT_FALSE(std::isnan(r.log10_upper_bound));
}

TEST(KStarWorkflow_SYNTHESIZED, BaselineVsFast_AgreeOnLog10KStarWithinTolerance_2RL0) {
    auto protein = loadEmat("test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin");
    auto ligand = loadEmat("test_data/2RL0.TestSimplePartitionFunction.ligand.emat.bin");
    auto complex = loadEmat("test_data/2RL0.TestSimplePartitionFunction.complex.emat.bin");

    if (!protein || !ligand || !complex) {
        GTEST_SKIP() << "Missing test data for 2RL0 emat triplet.\n"
                     << osprey::kstar::testutil::describeTestDataSearch("test_data/2RL0.TestSimplePartitionFunction.protein.emat.bin");
    }

    constexpr double eps = 0.8;

    KStarWorkflow<double> wf;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    opts.astar_variant = AStarVariant::Baseline;
    const auto base = wf.compute(*protein, *ligand, *complex, eps, PartitionFunctionMethod::AStar, opts);

    opts.astar_variant = AStarVariant::Fast;
    const auto fast = wf.compute(*protein, *ligand, *complex, eps, PartitionFunctionMethod::AStar, opts);

    // The partition function tests already check baseline/fast equivalence more tightly.
    // Here we only require the end-to-end workflow to stay close.
    EXPECT_NEAR(base.log10_lower_bound, fast.log10_lower_bound, 1e-6);
    EXPECT_NEAR(base.log10_upper_bound, fast.log10_upper_bound, 1e-6);
    EXPECT_NEAR(base.log10_value, fast.log10_value, 1e-6);
}

