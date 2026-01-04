#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <vector>

#include "energy_matrix.hpp"
#include "partition_function.hpp"

using namespace osprey::kstar;

namespace {

static int64_t totalConfs(const EnergyMatrix<double>& emat) {
    int64_t total = 1;
    const int32_t npos = emat.getNumPositions();
    for (int32_t pos = 0; pos < npos; ++pos) {
        total *= static_cast<int64_t>(emat.getNumConfsAtPos(pos));
    }
    return total;
}

} // namespace

TEST(PartitionFunction_Tutorial_GD, NoConformations_EarlyReturnNegInfBounds) {
    // If any position has 0 conformations, ConfSearch yields nothing.
    // The GD implementation should return early with log10(0) bounds.
    std::vector<int32_t> nrc = {0, 3};
    EnergyMatrix<double> emat(2, nrc);
    emat.setConstTerm(0.0);

    // Only set energies for the non-empty position.
    for (int32_t rc = 0; rc < 3; ++rc) {
        emat.setOneBody(1, rc, 0.0);
    }

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;
    opts.astar_variant = AStarVariant::Fast;

    const auto r = pfunc.compute(emat, /*epsilon=*/0.1, PartitionFunctionMethod::GradientDescent, opts);

    EXPECT_FALSE(r.converged);
    EXPECT_EQ(r.num_confs, 0);
    EXPECT_EQ(r.lower_bound, std::numeric_limits<double>::lowest());
    EXPECT_EQ(r.upper_bound, std::numeric_limits<double>::lowest());
}

TEST(PartitionFunction_Tutorial_GD, SingleConformation_ConvergesExact) {
    // With exactly one conformation, GD should converge exactly (LB==UB, delta==0).
    std::vector<int32_t> nrc = {1, 1, 1};
    EnergyMatrix<double> emat(3, nrc);
    emat.setConstTerm(0.0);
    emat.setOneBody(0, 0, 0.1);
    emat.setOneBody(1, 0, 0.2);
    emat.setOneBody(2, 0, 0.3);
    emat.setPairwise(1, 0, 0, 0, 0.01);
    emat.setPairwise(2, 0, 0, 0, 0.02);
    emat.setPairwise(2, 0, 1, 0, 0.03);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;
    opts.astar_variant = AStarVariant::Fast;

    const auto r = pfunc.compute(emat, /*epsilon=*/0.0, PartitionFunctionMethod::GradientDescent, opts);

    EXPECT_TRUE(r.converged);
    EXPECT_DOUBLE_EQ(r.delta, 0.0);
    EXPECT_DOUBLE_EQ(r.lower_bound, r.upper_bound);
    EXPECT_EQ(r.num_confs, 1);
    EXPECT_EQ(totalConfs(emat), 1);
}

TEST(PartitionFunction_Tutorial_GD, CantMakeProgressWhenRemainingIsPosInf_ReturnsNonConverged) {
    // Construct a tiny space with 2 conformations:
    // - one finite energy
    // - one +inf energy
    //
    // The GD score-batch stops early when it encounters +inf, leaving num_scored < total_confs.
    // After consuming the finite conformation, the buffer empties; the next score-batch sees +inf,
    // makes no progress, and the loop terminates non-converged with a conservative UB.
    std::vector<int32_t> nrc = {2};
    EnergyMatrix<double> emat(1, nrc);
    emat.setConstTerm(0.0);
    emat.setOneBody(0, 0, 0.0);
    emat.setOneBody(0, 1, std::numeric_limits<double>::infinity());

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;
    opts.astar_variant = AStarVariant::Fast;

    const auto r = pfunc.compute(emat, /*epsilon=*/0.0, PartitionFunctionMethod::GradientDescent, opts);

    EXPECT_FALSE(r.converged);
    EXPECT_EQ(r.num_confs, 1); // only the finite conformation was energied

    // Lower bound should be finite (the finite conformation contributes).
    EXPECT_TRUE(std::isfinite(r.lower_bound));

    // Upper bound should remain above lower bound (conservative due to remaining unknown conf).
    EXPECT_TRUE(std::isfinite(r.upper_bound));
    EXPECT_GT(r.upper_bound, r.lower_bound);
}

