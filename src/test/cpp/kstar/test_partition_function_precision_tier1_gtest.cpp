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

template<typename T>
static EnergyMatrix<T> permutePositions(const EnergyMatrix<T>& emat, const std::vector<int32_t>& perm_old_to_new) {
    const int32_t npos = emat.getNumPositions();
    std::vector<int32_t> nconfs_new(static_cast<std::size_t>(npos));
    for (int32_t old_pos = 0; old_pos < npos; ++old_pos) {
        const int32_t new_pos = perm_old_to_new[static_cast<std::size_t>(old_pos)];
        nconfs_new[static_cast<std::size_t>(new_pos)] = emat.getNumConfsAtPos(old_pos);
    }

    EnergyMatrix<T> out(npos, nconfs_new);
    out.setConstTerm(emat.getConstTerm());

    // One-body
    for (int32_t old_pos = 0; old_pos < npos; ++old_pos) {
        const int32_t new_pos = perm_old_to_new[static_cast<std::size_t>(old_pos)];
        const int32_t nconf = emat.getNumConfsAtPos(old_pos);
        for (int32_t rc = 0; rc < nconf; ++rc) {
            out.setOneBody(new_pos, rc, emat.getOneBody(old_pos, rc));
        }
    }

    // Pairwise: map (old_pos1, old_pos2) -> (new_pos1, new_pos2) and respect pos1 > pos2 requirement.
    for (int32_t old_pos1 = 1; old_pos1 < npos; ++old_pos1) {
        for (int32_t old_pos2 = 0; old_pos2 < old_pos1; ++old_pos2) {
            const int32_t np1 = perm_old_to_new[static_cast<std::size_t>(old_pos1)];
            const int32_t np2 = perm_old_to_new[static_cast<std::size_t>(old_pos2)];

            const int32_t nconf1 = emat.getNumConfsAtPos(old_pos1);
            const int32_t nconf2 = emat.getNumConfsAtPos(old_pos2);

            if (np1 == np2) {
                // Not a valid permutation.
                throw std::runtime_error("permutePositions: non-bijective perm");
            }

            const bool swapped = (np1 < np2);
            const int32_t new_pos1 = swapped ? np2 : np1;
            const int32_t new_pos2 = swapped ? np1 : np2;

            for (int32_t rc1 = 0; rc1 < nconf1; ++rc1) {
                for (int32_t rc2 = 0; rc2 < nconf2; ++rc2) {
                    const T e = emat.getPairwise(old_pos1, rc1, old_pos2, rc2);
                    if (!swapped) {
                        out.setPairwise(new_pos1, rc1, new_pos2, rc2, e);
                    } else {
                        // Swap rc indices to match new position ordering.
                        out.setPairwise(new_pos1, rc2, new_pos2, rc1, e);
                    }
                }
            }
        }
    }

    return out;
}

static void expectLEWithTol(double a, double b, double tol) {
    EXPECT_LE(a, b + tol);
}
static void expectGEWithTol(double a, double b, double tol) {
    EXPECT_GE(a, b - tol);
}

static double log10SumBoltzmann(const std::vector<double>& energies, double RT) {
    // Compute log10(sum_i exp(-E_i / RT)) in a numerically stable way.
    // Assumes energies is non-empty and finite.
    constexpr double ln10 = 2.3025850929940459;
    double max_neg = -energies[0] / RT;
    for (std::size_t i = 1; i < energies.size(); ++i) {
        const double v = -energies[i] / RT;
        if (v > max_neg) max_neg = v;
    }
    double sum = 0.0;
    for (double e : energies) {
        sum += std::exp((-e / RT) - max_neg);
    }
    return (max_neg + std::log(sum)) / ln10;
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

TEST(PartitionFunction_PRECISION_Tier1, AStarBoundsContainExactOracle_WhenEpsilonNonzero) {
    // For tiny spaces we can compute an oracle value by exact enumeration (epsilon=0 shortcut).
    // Then run A* in approximation mode (epsilon>0, exact enumeration disabled) and assert that:
    // - bounds are ordered
    // - bounds contain the oracle (true) value
    //
    // This validates bounding logic under nonzero epsilon, not just epsilon-monotonicity.
    std::mt19937_64 rng(0xA57AULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double oracle_eps = 0.0;
    constexpr double epsilon = 0.2;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        const auto oracle = pfunc.compute(emat, oracle_eps, PartitionFunctionMethod::AStar);
        ASSERT_TRUE(oracle.converged);
        EXPECT_DOUBLE_EQ(oracle.delta, 0.0);
        EXPECT_EQ(oracle.lower_bound, oracle.upper_bound);

        for (AStarVariant variant : {AStarVariant::Baseline, AStarVariant::Fast}) {
            opts.astar_variant = variant;
            const auto r = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

            SCOPED_TRACE(::testing::Message()
                         << "case=" << i
                         << " variant=" << (variant == AStarVariant::Baseline ? "Baseline" : "Fast")
                         << " epsilon=" << epsilon
                         << " oracle=" << oracle.lower_bound
                         << " r{lb=" << r.lower_bound << ", ub=" << r.upper_bound
                         << ", delta=" << r.delta << ", converged=" << r.converged
                         << ", confs=" << r.num_confs << "}");

            // Bound ordering must always hold.
            expectLEWithTol(r.lower_bound, r.upper_bound, 1e-12);

            // Oracle must be contained in bounds.
            expectLEWithTol(r.lower_bound, oracle.lower_bound, 1e-10);
            expectGEWithTol(r.upper_bound, oracle.lower_bound, 1e-10);

            EXPECT_GT(r.num_confs, 0);
        }
    }
}

TEST(PartitionFunction_PRECISION_Tier1, PermutationInvariance_AStarAndOracleOnTinySpaces) {
    // Permuting positions (with a consistent remapping of indices) must not change the partition function.
    // Use exact enumeration as the oracle (epsilon=0) and also check A* approximation bounds at epsilon>0.
    // Deterministic seed for reproducibility; change intentionally if updating expectations.
    std::mt19937_64 rng(0x0E6DULL);

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    constexpr double oracle_eps = 0.0;
    constexpr double epsilon = 0.2;
    constexpr int cases = 200;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);
        const int32_t npos = emat.getNumPositions();

        std::vector<int32_t> perm(static_cast<std::size_t>(npos));
        for (int32_t p = 0; p < npos; ++p) perm[static_cast<std::size_t>(p)] = p;
        std::shuffle(perm.begin(), perm.end(), rng);

        auto emat_perm = permutePositions(emat, perm);

        const auto oracle = pfunc.compute(emat, oracle_eps, PartitionFunctionMethod::AStar);
        const auto oracle2 = pfunc.compute(emat_perm, oracle_eps, PartitionFunctionMethod::AStar);
        ASSERT_TRUE(oracle.converged);
        ASSERT_TRUE(oracle2.converged);
        EXPECT_DOUBLE_EQ(oracle.delta, 0.0);
        EXPECT_DOUBLE_EQ(oracle2.delta, 0.0);

        SCOPED_TRACE(::testing::Message() << "case=" << i << " npos=" << npos);
        EXPECT_NEAR(oracle.lower_bound, oracle2.lower_bound, 1e-10);

        for (AStarVariant variant : {AStarVariant::Baseline, AStarVariant::Fast}) {
            opts.astar_variant = variant;
            const auto r1 = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);
            const auto r2 = pfunc.compute(emat_perm, epsilon, PartitionFunctionMethod::AStar, opts);

            SCOPED_TRACE(::testing::Message()
                         << "variant=" << (variant == AStarVariant::Baseline ? "Baseline" : "Fast")
                         << " epsilon=" << epsilon
                         << " r1{lb=" << r1.lower_bound << ", ub=" << r1.upper_bound << "}"
                         << " r2{lb=" << r2.lower_bound << ", ub=" << r2.upper_bound << "}");

            // Both runs must contain the same oracle value.
            expectLEWithTol(r1.lower_bound, oracle.lower_bound, 1e-10);
            expectGEWithTol(r1.upper_bound, oracle.lower_bound, 1e-10);
            expectLEWithTol(r2.lower_bound, oracle.lower_bound, 1e-10);
            expectGEWithTol(r2.upper_bound, oracle.lower_bound, 1e-10);
        }
    }
}

TEST(PartitionFunction_PRECISION_Tier1, PairwiseZeroFactorization_ClosedFormOracle) {
    // When all pairwise terms are zero, the partition function factorizes:
    //
    //   Z = exp(-E_const/RT) * Π_pos Σ_rc exp(-E_one(pos,rc)/RT)
    //
    // In log10-space:
    //
    //   log10(Z) = (-E_const/RT)/ln(10) + Σ_pos log10(Σ_rc exp(-E_one/RT))
    //
    // This is a closed-form oracle that catches sign/RT/log10 mistakes and missing term contributions.
    std::mt19937_64 rng(0xFACE0FFULL);
    std::uniform_real_distribution<double> one_body_dist(-8.0, 8.0);
    std::uniform_real_distribution<double> const_dist(-3.0, 3.0);
    std::uniform_int_distribution<int32_t> pos_dist(2, 6);
    std::uniform_int_distribution<int32_t> conf_dist(2, 4);

    constexpr double RT = 0.001987 * 298.15;
    constexpr double ln10 = 2.3025850929940459;
    constexpr int cases = 200;

    PartitionFunction<double> pfunc;
    PartitionFunction<double>::ComputeOptions opts;
    opts.allow_exact_enumeration = false;

    for (int i = 0; i < cases; ++i) {
        const int32_t npos = pos_dist(rng);
        std::vector<int32_t> nconfs(npos);
        for (int32_t pos = 0; pos < npos; ++pos) nconfs[pos] = conf_dist(rng);

        EnergyMatrix<double> emat(npos, nconfs);
        const double const_term = const_dist(rng);
        emat.setConstTerm(const_term);

        for (int32_t pos = 0; pos < npos; ++pos) {
            for (int32_t rc = 0; rc < nconfs[pos]; ++rc) {
                emat.setOneBody(pos, rc, one_body_dist(rng));
            }
        }
        // Pairwise = 0
        for (int32_t pos1 = 1; pos1 < npos; ++pos1) {
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                for (int32_t rc1 = 0; rc1 < nconfs[pos1]; ++rc1) {
                    for (int32_t rc2 = 0; rc2 < nconfs[pos2]; ++rc2) {
                        emat.setPairwise(pos1, rc1, pos2, rc2, 0.0);
                    }
                }
            }
        }

        // Closed-form oracle.
        double log10Z = (-const_term / RT) / ln10;
        for (int32_t pos = 0; pos < npos; ++pos) {
            std::vector<double> e;
            e.reserve(static_cast<std::size_t>(nconfs[pos]));
            for (int32_t rc = 0; rc < nconfs[pos]; ++rc) {
                e.push_back(emat.getOneBody(pos, rc));
            }
            log10Z += log10SumBoltzmann(e, RT);
        }

        // Exact enumeration (oracle path) should match.
        const auto exact = pfunc.compute(emat, 0.0, PartitionFunctionMethod::AStar);
        ASSERT_TRUE(exact.converged);
        EXPECT_DOUBLE_EQ(exact.delta, 0.0);
        EXPECT_EQ(exact.lower_bound, exact.upper_bound);

        SCOPED_TRACE(::testing::Message() << "case=" << i << " npos=" << npos << " log10Z=" << log10Z
                                          << " exact=" << exact.lower_bound);
        EXPECT_NEAR(exact.lower_bound, log10Z, 1e-10);

        // A* approximation mode (exact enumeration disabled) should also converge to the same value on tiny spaces.
        for (AStarVariant variant : {AStarVariant::Baseline, AStarVariant::Fast}) {
            opts.astar_variant = variant;
            const auto r = pfunc.compute(emat, 0.0, PartitionFunctionMethod::AStar, opts);
            EXPECT_TRUE(r.converged);
            EXPECT_NEAR(r.lower_bound, log10Z, 1e-10);
            EXPECT_NEAR(r.upper_bound, log10Z, 1e-10);
        }
    }
}

TEST(PartitionFunction_PRECISION_Tier1, Monotonicity_EnergyLowering_DoesNotDecreaseZ_ExactOracle) {
    // If any energy term decreases (making some conformations more favorable), the partition function Z must
    // not decrease. In log10-space, log10(Z) must be non-decreasing.
    //
    // Use exact enumeration (epsilon=0) as an oracle on tiny spaces.
    // Deterministic seed for reproducibility ("MONO" in hex).
    std::mt19937_64 rng(0x4D4F4E4FULL);
    std::uniform_real_distribution<double> delta_dist(0.5, 5.0); // magnitude of energy lowering
    std::uniform_int_distribution<int> which_dist(0, 2);         // 0=const, 1=one-body, 2=pairwise

    PartitionFunction<double> pfunc;
    constexpr double oracle_eps = 0.0;
    constexpr int cases = 400;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);
        const int32_t npos = emat.getNumPositions();

        const auto before = pfunc.compute(emat, oracle_eps, PartitionFunctionMethod::AStar);
        ASSERT_TRUE(before.converged);
        EXPECT_DOUBLE_EQ(before.delta, 0.0);
        EXPECT_EQ(before.lower_bound, before.upper_bound);

        auto emat2 = emat;
        const double delta = delta_dist(rng);

        const int which = which_dist(rng);
        if (which == 0) {
            // Lower const term.
            emat2.setConstTerm(emat.getConstTerm() - delta);
        } else if (which == 1) {
            // Lower a random one-body term.
            std::uniform_int_distribution<int32_t> pos_dist(0, npos - 1);
            const int32_t pos = pos_dist(rng);
            const int32_t nconf = emat.getNumConfsAtPos(pos);
            std::uniform_int_distribution<int32_t> rc_dist(0, nconf - 1);
            const int32_t rc = rc_dist(rng);
            emat2.setOneBody(pos, rc, emat.getOneBody(pos, rc) - delta);
        } else {
            // Lower a random pairwise term (pos1 > pos2).
            if (npos < 2) {
                continue;
            }
            std::uniform_int_distribution<int32_t> pos1_dist(1, npos - 1);
            const int32_t pos1 = pos1_dist(rng);
            std::uniform_int_distribution<int32_t> pos2_dist(0, pos1 - 1);
            const int32_t pos2 = pos2_dist(rng);
            const int32_t n1 = emat.getNumConfsAtPos(pos1);
            const int32_t n2 = emat.getNumConfsAtPos(pos2);
            std::uniform_int_distribution<int32_t> rc1_dist(0, n1 - 1);
            std::uniform_int_distribution<int32_t> rc2_dist(0, n2 - 1);
            const int32_t rc1 = rc1_dist(rng);
            const int32_t rc2 = rc2_dist(rng);
            emat2.setPairwise(pos1, rc1, pos2, rc2, emat.getPairwise(pos1, rc1, pos2, rc2) - delta);
        }

        const auto after = pfunc.compute(emat2, oracle_eps, PartitionFunctionMethod::AStar);
        ASSERT_TRUE(after.converged);
        EXPECT_DOUBLE_EQ(after.delta, 0.0);
        EXPECT_EQ(after.lower_bound, after.upper_bound);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " which=" << which
                                          << " delta=" << delta
                                          << " before=" << before.lower_bound
                                          << " after=" << after.lower_bound);

        // Allow equality (numeric dominance cutoffs may render tiny improvements indistinguishable).
        expectGEWithTol(after.lower_bound, before.lower_bound, 1e-10);
    }
}
