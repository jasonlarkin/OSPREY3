#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

#include "partition_function.hpp"
#include "energy_matrix.hpp"

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

TEST(PartitionFunction_PRECISION_Tier0, AStarEqualsExactEnumeration_OnTinySpaces_Double) {
    // Deterministic seed for reproducibility; change intentionally if updating expectations.
    std::mt19937_64 rng(0xC0FFEEULL);

    PartitionFunction<double> pfunc;
    constexpr double epsilon = 0.0; // forces full enumeration in A* loops (delta reaches 0 only when fully explored)
    constexpr int cases = 100;

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        // Oracle: exact enumeration shortcut (tiny spaces only).
        const auto exact = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar);

        PartitionFunction<double>::ComputeOptions opts;
        opts.allow_exact_enumeration = false;

        opts.astar_variant = AStarVariant::Baseline;
        const auto base = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

        opts.astar_variant = AStarVariant::Fast;
        const auto fast = pfunc.compute(emat, epsilon, PartitionFunctionMethod::AStar, opts);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " exact{lb=" << exact.lower_bound << ", ub=" << exact.upper_bound << "}"
                                          << " base{lb=" << base.lower_bound << ", ub=" << base.upper_bound << "}"
                                          << " fast{lb=" << fast.lower_bound << ", ub=" << fast.upper_bound << "}");

        ASSERT_TRUE(exact.converged);
        EXPECT_DOUBLE_EQ(exact.delta, 0.0);
        EXPECT_EQ(exact.lower_bound, exact.upper_bound);

        // With epsilon=0 and tiny spaces, both A* variants should match exact enumeration results.
        //
        // Note: do NOT assert base.num_confs == exact.num_confs. A* is allowed to stop early once the
        // remaining estimate becomes numerically negligible (the log-space add uses a dominance cutoff),
        // which can collapse upper==lower even when remaining_confs > 0.
        EXPECT_NEAR(base.lower_bound, exact.lower_bound, 1e-10);
        EXPECT_NEAR(base.upper_bound, exact.upper_bound, 1e-10);
        EXPECT_NEAR(fast.lower_bound, exact.lower_bound, 1e-10);
        EXPECT_NEAR(fast.upper_bound, exact.upper_bound, 1e-10);

        EXPECT_TRUE(base.converged);
        EXPECT_TRUE(fast.converged);
        EXPECT_NEAR(base.delta, 0.0, 1e-12);
        EXPECT_NEAR(fast.delta, 0.0, 1e-12);
        EXPECT_GT(base.num_confs, 0);
        EXPECT_GT(fast.num_confs, 0);
        EXPECT_LE(base.num_confs, exact.num_confs);
        EXPECT_LE(fast.num_confs, exact.num_confs);
    }
}

TEST(PartitionFunction_PRECISION_Tier0, FloatVsDouble_ExactEnumeration_CloseOnTinySpaces) {
    std::mt19937_64 rng(0xBADC0DEULL);

    PartitionFunction<double> pfunc_d;
    PartitionFunction<float> pfunc_f;

    constexpr double epsilon_d = 0.0;
    constexpr float epsilon_f = 0.0f;
    constexpr int cases = 100;

    for (int i = 0; i < cases; ++i) {
        auto emat_d = makeRandomTinyEnergyMatrix<double>(rng);

        // Re-create as float by copying energies through the public API.
        const int32_t npos = emat_d.getNumPositions();
        std::vector<int32_t> nconfs(npos);
        for (int32_t pos = 0; pos < npos; ++pos) {
            nconfs[pos] = emat_d.getNumConfsAtPos(pos);
        }
        EnergyMatrix<float> emat_f(npos, nconfs);
        emat_f.setConstTerm(static_cast<float>(emat_d.getConstTerm()));
        for (int32_t pos = 0; pos < npos; ++pos) {
            for (int32_t c = 0; c < nconfs[pos]; ++c) {
                emat_f.setOneBody(pos, c, static_cast<float>(emat_d.getOneBody(pos, c)));
            }
        }
        for (int32_t pos1 = 1; pos1 < npos; ++pos1) {
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                for (int32_t c1 = 0; c1 < nconfs[pos1]; ++c1) {
                    for (int32_t c2 = 0; c2 < nconfs[pos2]; ++c2) {
                        emat_f.setPairwise(pos1, c1, pos2, c2, static_cast<float>(emat_d.getPairwise(pos1, c1, pos2, c2)));
                    }
                }
            }
        }

        const auto exact_d = pfunc_d.compute(emat_d, epsilon_d, PartitionFunctionMethod::AStar);
        const auto exact_f = pfunc_f.compute(emat_f, epsilon_f, PartitionFunctionMethod::AStar);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " exact_d=" << exact_d.lower_bound
                                          << " exact_f=" << exact_f.lower_bound);

        ASSERT_TRUE(std::isfinite(exact_d.lower_bound));
        ASSERT_TRUE(std::isfinite(exact_f.lower_bound));

        // Float should be close to double for tiny spaces; tolerate some drift.
        EXPECT_NEAR(static_cast<double>(exact_f.lower_bound), exact_d.lower_bound, 1e-3);
    }
}

TEST(PartitionFunction_PRECISION_Tier0, GradientDescentBoundsContainExactEnumeration_OnTinySpaces) {
    std::mt19937_64 rng(0x61D0ULL);

    PartitionFunction<double> pfunc;

    // Epsilon=0 gives an exact oracle via enumeration (upper==lower, delta==0).
    constexpr double oracle_eps = 0.0;

    // Use a non-zero epsilon for GD to exercise its approximation behavior.
    // We only require the true value to be contained in its bounds.
    constexpr double gd_eps = 0.2;
    constexpr int cases = 200;

    PartitionFunction<double>::ComputeOptions gd_opts;
    gd_opts.allow_exact_enumeration = false; // avoid accidentally bypassing GD on tiny spaces

    for (int i = 0; i < cases; ++i) {
        auto emat = makeRandomTinyEnergyMatrix<double>(rng);

        const auto exact = pfunc.compute(emat, oracle_eps, PartitionFunctionMethod::AStar);
        const auto gd = pfunc.compute(emat, gd_eps, PartitionFunctionMethod::GradientDescent, gd_opts);

        SCOPED_TRACE(::testing::Message() << "case=" << i
                                          << " exact=" << exact.lower_bound
                                          << " gd{lb=" << gd.lower_bound << ", ub=" << gd.upper_bound
                                          << ", delta=" << gd.delta << ", converged=" << gd.converged
                                          << ", confs=" << gd.num_confs << "}");

        ASSERT_TRUE(exact.converged);
        EXPECT_DOUBLE_EQ(exact.delta, 0.0);
        EXPECT_EQ(exact.lower_bound, exact.upper_bound);

        // Bound ordering must hold.
        EXPECT_LE(gd.lower_bound, gd.upper_bound + 1e-12);

        // True value (exact) must be within GD bounds.
        EXPECT_LE(gd.lower_bound, exact.lower_bound + 1e-10);
        EXPECT_GE(gd.upper_bound, exact.lower_bound - 1e-10);

        // Sanity: should explore at least one conformation.
        EXPECT_GT(gd.num_confs, 0);
    }
}
