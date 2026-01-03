#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "energy_matrix.hpp"

using namespace osprey::kstar;

TEST(EnergyMatrix_SYNTHESIZED, BasicGetSet) {
    std::vector<int32_t> num_confs = {2, 2, 2};
    EnergyMatrix<double> emat(3, num_confs);

    emat.setOneBody(0, 0, 1.0);
    emat.setOneBody(0, 1, 2.0);
    emat.setOneBody(1, 0, 3.0);
    emat.setOneBody(1, 1, 4.0);
    emat.setOneBody(2, 0, 5.0);
    emat.setOneBody(2, 1, 6.0);

    EXPECT_DOUBLE_EQ(emat.getOneBody(0, 0), 1.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(0, 1), 2.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(1, 0), 3.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(1, 1), 4.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(2, 0), 5.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(2, 1), 6.0);

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

    EXPECT_DOUBLE_EQ(emat.getPairwise(1, 0, 0, 0), 10.0);
    EXPECT_DOUBLE_EQ(emat.getPairwise(0, 0, 1, 0), 10.0);
    EXPECT_DOUBLE_EQ(emat.getPairwise(1, 0, 0, 1), 11.0);
    EXPECT_DOUBLE_EQ(emat.getPairwise(0, 1, 1, 0), 11.0);
    EXPECT_DOUBLE_EQ(emat.getPairwise(2, 0, 1, 0), 30.0);
    EXPECT_DOUBLE_EQ(emat.getPairwise(1, 0, 2, 0), 30.0);
}

TEST(EnergyMatrix_SYNTHESIZED, ComputeEnergy) {
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
    EXPECT_NEAR(emat.computeEnergy({0, 1}), 17.0, 1e-10);
    EXPECT_NEAR(emat.computeEnergy({1, 0}), 16.0, 1e-10);
    EXPECT_NEAR(emat.computeEnergy({1, 1}), 19.0, 1e-10);
}

TEST(EnergyMatrix_SYNTHESIZED, ConstTerm) {
    std::vector<int32_t> num_confs = {2, 2};
    EnergyMatrix<double> emat(2, num_confs);

    EXPECT_DOUBLE_EQ(emat.getConstTerm(), 0.0);
    emat.setConstTerm(5.0);
    EXPECT_DOUBLE_EQ(emat.getConstTerm(), 5.0);

    emat.setOneBody(0, 0, 1.0);
    emat.setOneBody(1, 0, 2.0);
    EXPECT_NEAR(emat.computeEnergy({0, 0}), 8.0, 1e-10);
}

TEST(EnergyMatrix_SYNTHESIZED, VariableConfsPerPos) {
    std::vector<int32_t> num_confs = {2, 3, 4};
    EnergyMatrix<double> emat(3, num_confs);

    EXPECT_EQ(emat.getNumConfsAtPos(0), 2);
    EXPECT_EQ(emat.getNumConfsAtPos(1), 3);
    EXPECT_EQ(emat.getNumConfsAtPos(2), 4);

    emat.setOneBody(1, 2, 5.0);
    emat.setOneBody(2, 3, 9.0);

    EXPECT_DOUBLE_EQ(emat.getOneBody(1, 2), 5.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(2, 3), 9.0);
}

TEST(EnergyMatrix_SYNTHESIZED, BoundsChecking) {
    std::vector<int32_t> num_confs = {2, 2};
    EnergyMatrix<double> emat(2, num_confs);

    emat.setOneBody(0, 0, 1.0);
    EXPECT_DOUBLE_EQ(emat.getOneBody(0, 0), 1.0);

    EXPECT_THROW((void)emat.getOneBody(2, 0), std::out_of_range);
    EXPECT_THROW((void)emat.getOneBody(0, 2), std::out_of_range);
}

