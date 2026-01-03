#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "energy_matrix.hpp"
#include "partition_function.hpp"

using namespace osprey::kstar;

template<typename T>
static EnergyMatrix<T> makeRandomTinyEnergyMatrix(std::mt19937_64& rng) {
    std::uniform_int_distribution<int32_t> pos_dist(2, 5);
    std::uniform_int_distribution<int32_t> conf_dist(2, 3);
    std::uniform_real_distribution<double> one_body_dist(-5.0, 5.0);
    std::uniform_real_distribution<double> pairwise_dist(-1.0, 1.0);
    std::uniform_real_distribution<double> const_dist(-2.0, 2.0);

    const int32_t num_positions = pos_dist(rng);
    std::vector<int32_t> num_confs_per_pos(num_positions);
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        num_confs_per_pos[pos] = conf_dist(rng);
    }

    EnergyMatrix<T> emat(num_positions, num_confs_per_pos);
    emat.setConstTerm(static_cast<T>(const_dist(rng)));

    for (int32_t pos = 0; pos < num_positions; ++pos) {
        for (int32_t conf = 0; conf < num_confs_per_pos[pos]; ++conf) {
            emat.setOneBody(pos, conf, static_cast<T>(one_body_dist(rng)));
        }
    }

    for (int32_t pos1 = 1; pos1 < num_positions; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            for (int32_t conf1 = 0; conf1 < num_confs_per_pos[pos1]; ++conf1) {
                for (int32_t conf2 = 0; conf2 < num_confs_per_pos[pos2]; ++conf2) {
                    emat.setPairwise(pos1, conf1, pos2, conf2, static_cast<T>(pairwise_dist(rng)));
                }
            }
        }
    }

    return emat;
}

static void expectLEWithTol(double a, double b, double tol) {
    EXPECT_LE(a, b + tol);
}
static void expectGEWithTol(double a, double b, double tol) {
    EXPECT_GE(a, b - tol);
}

TEST(PartitionFunction_PRECISION_Tier1, EpsilonMonotonicity_AStarBounds_BaselineAndFast) {
    std::mt19937_64 rng(0xA11CEULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double eps_lo = 0.05;
    constexpr double eps_hi = 0.5;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        for (AStarVariant variant : {AStarVariant::Baseline, AStarVariant::Fast}) {
            opts.astar_variant = variant;

            const auto r_hi = pfunc.compute(emat, eps_hi, PartitionFunctionMethod::AStar, opts);
            const auto r_lo = pfunc.compute(emat, eps_lo, PartitionFunctionMethod::AStar, opts);

            SCOPED_TRACE(::testing::Message() << "case=" << i
                                              << " variant=" << (variant == AStarVariant::Baseline ? "Baseline" : "Fast")
                                              << " eps_hi=" << eps_hi << " eps_lo=" << eps_lo
                                              << " hi{lb=" << r_hi.lower_bound << ", ub=" << r_hi.upper_bound << ", delta=" << r_hi.delta << "}"
                                              << " lo{lb=" << r_lo.lower_bound << ", ub=" << r_lo.upper_bound << ", delta=" << r_lo.delta << "}");

            // More stringent epsilon should not produce a worse lower bound.
            expectGEWithTol(r_lo.lower_bound, r_hi.lower_bound, 1e-12);
            // More stringent epsilon should not produce a worse upper bound (should tighten or equal).
            expectLEWithTol(r_lo.upper_bound, r_hi.upper_bound, 1e-12);

            // Both bounds should remain ordered.
            expectLEWithTol(r_lo.lower_bound, r_lo.upper_bound, 1e-12);
            expectLEWithTol(r_hi.lower_bound, r_hi.upper_bound, 1e-12);
        }
    }
}

TEST(PartitionFunction_PRECISION_Tier1, Determinism_AStarBaselineAndFast) {
    std::mt19937_64 rng(0xD371ULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double epsilon = 0.2;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        for (AStarVariant variant : {AStarVariant::Baseline, AStarVariant::Fast}) {
            opts.astar_variant = variant;

            const auto r1 = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);
            const auto r2 = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

            SCOPED_TRACE(::testing::Message() << "case=" << i
                                              << " variant=" << (variant == AStarVariant::Baseline ? "Baseline" : "Fast")
                                              << " epsilon=" << epsilon
                                              << " r1{lb=" << r1.lower_bound << ", ub=" << r1.upper_bound << ", delta=" << r1.delta << ", confs=" << r1.num_confs << "}"
                                              << " r2{lb=" << r2.lower_bound << ", ub=" << r2.upper_bound << ", delta=" << r2.delta << ", confs=" << r2.num_confs << "}");

            EXPECT_NEAR(r1.lower_bound, r2.lower_bound, 1e-12);
            EXPECT_NEAR(r1.upper_bound, r2.upper_bound, 1e-12);
            EXPECT_NEAR(r1.delta, r2.delta, 1e-12);
            EXPECT_EQ(r1.converged, r2.converged);
            EXPECT_EQ(r1.num_confs, r2.num_confs);
        }
    }
}

TEST(PartitionFunction_PRECISION_Tier1, BaselineFastEquivalence_RandomTinyEnergyMatrices) {
    std::mt19937_64 rng(0xFA57ULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double epsilon = 0.2;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        opts.astar_variant = AStarVariant::Baseline;
        const auto base = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

        opts.astar_variant = AStarVariant::Fast;
        const auto fast = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " epsilon=" << epsilon
                                          << " base{lb=" << base.lower_bound << ", ub=" << base.upper_bound
                                          << ", delta=" << base.delta << ", converged=" << base.converged
                                          << ", confs=" << base.num_confs << "}"
                                          << " fast{lb=" << fast.lower_bound << ", ub=" << fast.upper_bound
                                          << ", delta=" << fast.delta << ", converged=" << fast.converged
                                          << ", confs=" << fast.num_confs << "}");

        EXPECT_NEAR(base.lower_bound, fast.lower_bound, 1e-10);
        EXPECT_NEAR(base.upper_bound, fast.upper_bound, 1e-10);
        EXPECT_NEAR(base.delta, fast.delta, 1e-12);
        EXPECT_EQ(base.converged, fast.converged);
        EXPECT_EQ(base.num_confs, fast.num_confs);
    }
}

TEST(PartitionFunction_PRECISION_Tier1, EpsilonMonotonicity_GradientDescentBounds) {
    std::mt19937_64 rng(0x6D6D6DULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double eps_lo = 0.05;
    constexpr double eps_hi = 0.5;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        const auto r_hi = pfunc.compute(emat, eps_hi, PartitionFunctionMethod::GradientDescent, opts);
        const auto r_lo = pfunc.compute(emat, eps_lo, PartitionFunctionMethod::GradientDescent, opts);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " eps_hi=" << eps_hi << " eps_lo=" << eps_lo
                                          << " hi{lb=" << r_hi.lower_bound << ", ub=" << r_hi.upper_bound << ", delta=" << r_hi.delta << "}"
                                          << " lo{lb=" << r_lo.lower_bound << ", ub=" << r_lo.upper_bound << ", delta=" << r_lo.delta << "}");

        // More stringent epsilon should not produce a worse lower bound.
        expectGEWithTol(r_lo.lower_bound, r_hi.lower_bound, 1e-12);
        // More stringent epsilon should not produce a worse upper bound (should tighten or equal).
        expectLEWithTol(r_lo.upper_bound, r_hi.upper_bound, 1e-12);

        // Both bounds should remain ordered.
        expectLEWithTol(r_lo.lower_bound, r_lo.upper_bound, 1e-12);
        expectLEWithTol(r_hi.lower_bound, r_hi.upper_bound, 1e-12);
    }
}

TEST(PartitionFunction_PRECISION_Tier1, Determinism_GradientDescent) {
    std::mt19937_64 rng(0x6D6D6EULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double epsilon = 0.2;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        const auto r1 = pfunc.compute(emat, epsilon, PartitionFunctionMethod::GradientDescent, opts);
        const auto r2 = pfunc.compute(emat, epsilon, PartitionFunctionMethod::GradientDescent, opts);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " epsilon=" << epsilon
                                          << " r1{lb=" << r1.lower_bound << ", ub=" << r1.upper_bound << ", delta=" << r1.delta << ", confs=" << r1.num_confs << "}"
                                          << " r2{lb=" << r2.lower_bound << ", ub=" << r2.upper_bound << ", delta=" << r2.delta << ", confs=" << r2.num_confs << "}");

        EXPECT_NEAR(r1.lower_bound, r2.lower_bound, 1e-12);
        EXPECT_NEAR(r1.upper_bound, r2.upper_bound, 1e-12);
        EXPECT_NEAR(r1.delta, r2.delta, 1e-12);
        EXPECT_EQ(r1.converged, r2.converged);
        EXPECT_EQ(r1.num_confs, r2.num_confs);
    }
}

TEST(PartitionFunction_PRECISION_Tier1, ConstTermShiftInvariance_GradientDescentBounds) {
    // Same invariance as the exact-enumeration oracle test, but asserted for the GradientDescent method:
    // shifting all energies by c should shift log10(Q) by -c/(RT*ln(10)).
    //
    // For an algorithmic approximation, this should still hold because every Boltzmann weight is multiplied
    // by the same factor exp(-c/RT), so both lower and upper bounds should shift equally and delta should
    // remain unchanged (up to floating error).
    std::mt19937_64 rng(0x6D6D6FULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false; // exercise GD, not the exact-enum shortcut

    constexpr double epsilon = 0.2;
    constexpr double RT = 0.001987 * 298.15;
    constexpr double ln10 = 2.3025850929940459;
    constexpr int cases = 200;

    std::uniform_real_distribution<double> shift_dist(-2.0, 2.0);

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);
        const double c = shift_dist(rng);

        auto emat_shift = emat;
        emat_shift.setConstTerm(emat.getConstTerm() + c);

        const auto r1 = pfunc.compute(emat, epsilon, PartitionFunctionMethod::GradientDescent, opts);
        const auto r2 = pfunc.compute(emat_shift, epsilon, PartitionFunctionMethod::GradientDescent, opts);

        const double expected_shift = -c / (RT * ln10);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " epsilon=" << epsilon
                                          << " c=" << c
                                          << " expected_shift=" << expected_shift
                                          << " r1{lb=" << r1.lower_bound << ", ub=" << r1.upper_bound << ", delta=" << r1.delta << ", confs=" << r1.num_confs << "}"
                                          << " r2{lb=" << r2.lower_bound << ", ub=" << r2.upper_bound << ", delta=" << r2.delta << ", confs=" << r2.num_confs << "}");

        EXPECT_NEAR(r2.lower_bound - r1.lower_bound, expected_shift, 1e-10);
        EXPECT_NEAR(r2.upper_bound - r1.upper_bound, expected_shift, 1e-10);
        EXPECT_NEAR(r2.delta, r1.delta, 1e-12);
        EXPECT_EQ(r2.converged, r1.converged);
        EXPECT_EQ(r2.num_confs, r1.num_confs);
    }
}

TEST(PartitionFunction_PRECISION_Tier1, ConstTermShiftInvariance_ExactEnumerationOracle) {
    // If we add a constant c to every conformation energy, then:
    //   Q' = sum exp(-(E+c)/RT) = exp(-c/RT) * Q
    // So in log10-space:
    //   log10(Q') = log10(Q) - c/(RT*ln(10))
    //
    // We test using exact enumeration (oracle) on tiny spaces.
    std::mt19937_64 rng(0x5A1F7ULL);

    PartitionFunction<double> pfunc;

    constexpr double RT = 0.001987 * 298.15;
    constexpr double ln10 = 2.3025850929940459;
    constexpr int cases = 200;

    std::uniform_real_distribution<double> shift_dist(-2.0, 2.0);

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);
        const double c = shift_dist(rng);

        auto emat_shift = emat;
        emat_shift.setConstTerm(emat.getConstTerm() + c);

        const auto r = pfunc.compute(emat, 0.0, PartitionFunctionMethod::AStar);
        const auto r2 = pfunc.compute(emat_shift, 0.0, PartitionFunctionMethod::AStar);

        ASSERT_TRUE(r.converged);
        ASSERT_TRUE(r2.converged);
        EXPECT_DOUBLE_EQ(r.delta, 0.0);
        EXPECT_DOUBLE_EQ(r2.delta, 0.0);

        const double expected_shift = -c / (RT * ln10);
        const double observed_shift = r2.lower_bound - r.lower_bound;

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " c=" << c
                                          << " expected_shift=" << expected_shift
                                          << " observed_shift=" << observed_shift
                                          << " r.lb=" << r.lower_bound
                                          << " r2.lb=" << r2.lower_bound);

        EXPECT_NEAR(observed_shift, expected_shift, 1e-10);
    }
}

