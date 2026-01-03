#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "energy_matrix.hpp"

using namespace osprey::kstar;

TEST(EnergyMatrix_OSPREYPattern_SYNTHESIZED, ConfEPatternMatchesExpectedSummation) {
    std::vector<int32_t> num_confs = {2, 2, 2};
    EnergyMatrix<double> emat(3, num_confs);

    emat.setOneBody(0, 0, 1.0);
    emat.setOneBody(0, 1, 2.0);
    emat.setOneBody(1, 0, 3.0);
    emat.setOneBody(1, 1, 4.0);
    emat.setOneBody(2, 0, 5.0);
    emat.setOneBody(2, 1, 6.0);

    emat.setPairwise(1, 0, 0, 0, 10.0);
    emat.setPairwise(1, 0, 0, 1, 11.0);
    emat.setPairwise(1, 1, 0, 0, 12.0);
    emat.setPairwise(1, 1, 0, 1, 13.0);

    emat.setPairwise(2, 0, 0, 0, 20.0);
    emat.setPairwise(2, 0, 0, 1, 21.0);
    emat.setPairwise(2, 1, 0, 0, 22.0);
    emat.setPairwise(2, 1, 0, 1, 23.0);

    emat.setPairwise(2, 0, 1, 0, 30.0);
    emat.setPairwise(2, 0, 1, 1, 31.0);
    emat.setPairwise(2, 1, 1, 0, 32.0);
    emat.setPairwise(2, 1, 1, 1, 33.0);

    std::vector<int32_t> conf1 = {0, 0, 0};
    double expected1 = 1.0 + 3.0 + 5.0 + 10.0 + 20.0 + 30.0;
    EXPECT_NEAR(emat.computeEnergy(conf1), expected1, 1e-10);

    std::vector<int32_t> conf2 = {1, 1, 1};
    double expected2 = 2.0 + 4.0 + 6.0 + 13.0 + 23.0 + 33.0;
    EXPECT_NEAR(emat.computeEnergy(conf2), expected2, 1e-10);

    emat.setConstTerm(5.0);
    EXPECT_NEAR(emat.computeEnergy(conf1), expected1 + 5.0, 1e-10);
}

TEST(EnergyMatrix_OSPREYPattern_SYNTHESIZED, GetInternalEnergyPatternExample) {
    std::vector<int32_t> num_confs = {2, 2};
    EnergyMatrix<double> emat(2, num_confs);

    emat.setOneBody(0, 0, 1.0);
    emat.setOneBody(0, 1, 2.0);
    emat.setOneBody(1, 0, 3.0);
    emat.setOneBody(1, 1, 4.0);

    emat.setPairwise(1, 0, 0, 0, 10.0);
    emat.setPairwise(1, 0, 0, 1, 11.0);
    emat.setPairwise(1, 1, 0, 0, 12.0);
    emat.setPairwise(1, 1, 0, 1, 13.0);

    EXPECT_NEAR(emat.computeEnergy({0, 0}), 14.0, 1e-10);
}

TEST(EnergyMatrix_OSPREYPattern_SYNTHESIZED, PairwiseSymmetry) {
    std::vector<int32_t> num_confs = {2, 2};
    EnergyMatrix<double> emat(2, num_confs);

    emat.setPairwise(1, 0, 0, 1, 11.0);

    double e1 = emat.getPairwise(1, 0, 0, 1);
    double e2 = emat.getPairwise(0, 1, 1, 0);
    EXPECT_NEAR(e1, e2, 1e-10);
    EXPECT_NEAR(e1, 11.0, 1e-10);
}

