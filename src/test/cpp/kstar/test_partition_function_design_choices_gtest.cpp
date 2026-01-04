#include <gtest/gtest.h>

#include "partition_function.hpp"
#include "energy_matrix.hpp"

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

static EnergyMatrix<double> makeTinyEmat() {
    // 3 positions, 3 RC each => 27 conformations, well below exact-enum gate.
    std::vector<int32_t> nrc = {3, 3, 3};
    EnergyMatrix<double> emat(3, nrc);
    emat.setConstTerm(0.0);
    for (int32_t pos = 0; pos < 3; ++pos) {
        for (int32_t rc = 0; rc < 3; ++rc) {
            emat.setOneBody(pos, rc, 0.1 * (pos + 1) + 0.01 * rc);
        }
    }
    for (int32_t pos1 = 1; pos1 < 3; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            for (int32_t rc1 = 0; rc1 < 3; ++rc1) {
                for (int32_t rc2 = 0; rc2 < 3; ++rc2) {
                    emat.setPairwise(pos1, rc1, pos2, rc2, 0.001 * (rc1 - rc2));
                }
            }
        }
    }
    return emat;
}

} // namespace

TEST(PartitionFunction_DesignChoices, ExactEnumerationGateEnabled_ReturnsExact) {
    const auto emat = makeTinyEmat();

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = true;
    opts.astar_variant = AStarVariant::Fast;

    const auto r = pfunc.compute(emat, /*epsilon=*/1e-6, PartitionFunctionMethod::AStar, opts);

    EXPECT_TRUE(r.converged);
    EXPECT_DOUBLE_EQ(r.delta, 0.0);
    EXPECT_DOUBLE_EQ(r.lower_bound, r.upper_bound);
    EXPECT_EQ(r.num_confs, totalConfs(emat));
}

TEST(PartitionFunction_DesignChoices, EpsilonZeroForcesFullEnumerationWhenExactEnumDisabled) {
    const auto emat = makeTinyEmat();

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false; // force the A* path even though the space is tiny
    opts.astar_variant = AStarVariant::Fast;

    const auto r = pfunc.compute(emat, /*epsilon=*/0.0, PartitionFunctionMethod::AStar, opts);

    // With epsilon==0, convergence requires lower==upper, which in this implementation requires exhausting the space.
    EXPECT_TRUE(r.converged);
    EXPECT_DOUBLE_EQ(r.delta, 0.0);
    EXPECT_DOUBLE_EQ(r.lower_bound, r.upper_bound);
    EXPECT_EQ(r.num_confs, totalConfs(emat));
}

