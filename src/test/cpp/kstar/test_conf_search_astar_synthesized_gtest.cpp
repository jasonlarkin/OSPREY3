#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "conf_search_astar.hpp"
#include "energy_matrix.hpp"

using namespace osprey::kstar;

namespace {

static EnergyMatrix<double> makeTinyEmat4pos_2rc() {
    // 4 positions, 2 RCs each: total confs = 16
    EnergyMatrix<double> emat(4, {2, 2, 2, 2});
    emat.setConstTerm(0.0);
    // Deterministic small energies
    for (int pos = 0; pos < 4; ++pos) {
        for (int rc = 0; rc < 2; ++rc) {
            emat.setOneBody(pos, rc, 0.1 * (pos + 1) + 0.01 * rc);
        }
    }
    for (int pos1 = 1; pos1 < 4; ++pos1) {
        for (int pos2 = 0; pos2 < pos1; ++pos2) {
            for (int rc1 = 0; rc1 < 2; ++rc1) {
                for (int rc2 = 0; rc2 < 2; ++rc2) {
                    emat.setPairwise(pos1, rc1, pos2, rc2, 0.001 * (pos1 + pos2 + rc1 + rc2));
                }
            }
        }
    }
    return emat;
}

static EnergyMatrix<double> makeTieHeavyEmat4pos_2rc() {
    // All energies equal (ties everywhere).
    EnergyMatrix<double> emat(4, {2, 2, 2, 2});
    emat.setConstTerm(0.0);
    for (int pos = 0; pos < 4; ++pos) {
        for (int rc = 0; rc < 2; ++rc) {
            emat.setOneBody(pos, rc, 0.0);
        }
    }
    for (int pos1 = 1; pos1 < 4; ++pos1) {
        for (int pos2 = 0; pos2 < pos1; ++pos2) {
            for (int rc1 = 0; rc1 < 2; ++rc1) {
                for (int rc2 = 0; rc2 < 2; ++rc2) {
                    emat.setPairwise(pos1, rc1, pos2, rc2, 0.0);
                }
            }
        }
    }
    return emat;
}

static std::uint64_t encodeAssignments(const std::vector<int32_t>& a) {
    // pack 4 positions with 2 bits each
    std::uint64_t x = 0;
    for (size_t i = 0; i < a.size(); ++i) {
        x |= (static_cast<std::uint64_t>(a[i]) & 0x3ULL) << (2 * i);
    }
    return x;
}

static void assertConfSearchBasicProperties(ConfSearch& search, std::uint64_t expected_total) {
    EXPECT_EQ(search.getNumConformations(), expected_total);

    std::unordered_set<std::uint64_t> seen;
    seen.reserve(static_cast<size_t>(expected_total));

    double prev = -1e300;
    std::uint64_t count = 0;
    while (true) {
        auto c = search.nextConf();
        if (!c) {
            break;
        }
        ASSERT_EQ(c->assignments.size(), 4u);
        EXPECT_GE(c->score, prev - 1e-12); // non-decreasing (ties ok)
        prev = c->score;

        const auto key = encodeAssignments(c->assignments);
        EXPECT_TRUE(seen.insert(key).second);
        ++count;
        if (count > expected_total + 1) {
            FAIL() << "ConfSearch returned more conformations than expected";
        }
    }

    EXPECT_EQ(count, expected_total);
}

static std::vector<double> collectAllScores(ConfSearch& search) {
    std::vector<double> scores;
    scores.reserve(static_cast<std::size_t>(search.getNumConformations()));
    while (true) {
        auto c = search.nextConf();
        if (!c) break;
        scores.push_back(c->score);
    }
    return scores;
}

static std::vector<double> enumerateAllEnergies(const EnergyMatrix<double>& emat) {
    // Assumes 4 positions, 2 RCs each (16 conformations).
    std::vector<double> energies;
    energies.reserve(16);
    std::vector<int32_t> conf(4, 0);
    for (int a = 0; a < 2; ++a) {
        conf[0] = a;
        for (int b = 0; b < 2; ++b) {
            conf[1] = b;
            for (int c = 0; c < 2; ++c) {
                conf[2] = c;
                for (int d = 0; d < 2; ++d) {
                    conf[3] = d;
                    energies.push_back(emat.computeEnergy(conf));
                }
            }
        }
    }
    return energies;
}

static EnergyMatrix<double> permutePositions(const EnergyMatrix<double>& emat, const std::vector<int32_t>& perm) {
    // newPos i corresponds to oldPos perm[i]
    const int32_t n = static_cast<int32_t>(perm.size());
    std::vector<int32_t> num_confs;
    num_confs.reserve(static_cast<std::size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        num_confs.push_back(emat.getNumConfsAtPos(perm[static_cast<std::size_t>(i)]));
    }

    EnergyMatrix<double> out(n, num_confs);
    out.setConstTerm(emat.getConstTerm());

    for (int32_t i = 0; i < n; ++i) {
        const int32_t oldPos = perm[static_cast<std::size_t>(i)];
        for (int32_t rc = 0; rc < num_confs[static_cast<std::size_t>(i)]; ++rc) {
            out.setOneBody(i, rc, emat.getOneBody(oldPos, rc));
        }
    }

    for (int32_t i = 1; i < n; ++i) {
        const int32_t oldI = perm[static_cast<std::size_t>(i)];
        const int32_t ni = num_confs[static_cast<std::size_t>(i)];
        for (int32_t j = 0; j < i; ++j) {
            const int32_t oldJ = perm[static_cast<std::size_t>(j)];
            const int32_t nj = num_confs[static_cast<std::size_t>(j)];
            for (int32_t rci = 0; rci < ni; ++rci) {
                for (int32_t rcj = 0; rcj < nj; ++rcj) {
                    out.setPairwise(i, rci, j, rcj, emat.getPairwise(oldI, rci, oldJ, rcj));
                }
            }
        }
    }
    return out;
}

static void assertSameScoresSorted(std::vector<double> a, std::vector<double> b, double tol) {
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_LE(std::fabs(a[i] - b[i]), tol);
    }
}

} // namespace

TEST(ConfSearchAStar_SYNTHESIZED, BaselineEnumeratesAllConfsMonotonicScore) {
    auto emat = makeTinyEmat4pos_2rc();
    AStarConfSearchBaseline<double> search(emat);
    assertConfSearchBasicProperties(search, 16);
}

TEST(ConfSearchAStar_SYNTHESIZED, FastEnumeratesAllConfsMonotonicScore) {
    auto emat = makeTinyEmat4pos_2rc();
    AStarConfSearchFast<double> search(emat);
    assertConfSearchBasicProperties(search, 16);
}

TEST(ConfSearchAStar_SYNTHESIZED, TinySpaceOracleMatchesExactEnergies) {
    auto emat = makeTinyEmat4pos_2rc();
    auto oracle = enumerateAllEnergies(emat);
    std::sort(oracle.begin(), oracle.end());

    AStarConfSearchBaseline<double> baseline(emat);
    auto bs = collectAllScores(baseline);
    std::sort(bs.begin(), bs.end());
    ASSERT_EQ(bs.size(), oracle.size());
    for (std::size_t i = 0; i < oracle.size(); ++i) {
        ASSERT_LE(std::fabs(bs[i] - oracle[i]), 1e-12);
    }

    AStarConfSearchFast<double> fast(emat);
    auto fs = collectAllScores(fast);
    std::sort(fs.begin(), fs.end());
    ASSERT_EQ(fs.size(), oracle.size());
    for (std::size_t i = 0; i < oracle.size(); ++i) {
        ASSERT_LE(std::fabs(fs[i] - oracle[i]), 1e-12);
    }
}

TEST(ConfSearchAStar_SYNTHESIZED, Metamorphic_PermutationInvariance_ScoreMultiset) {
    const auto emat = makeTinyEmat4pos_2rc();
    const std::vector<int32_t> perm = {2, 0, 3, 1};
    const auto emat2 = permutePositions(emat, perm);

    AStarConfSearchBaseline<double> s1(emat);
    AStarConfSearchBaseline<double> s2(emat2);
    assertSameScoresSorted(collectAllScores(s1), collectAllScores(s2), 1e-12);

    AStarConfSearchFast<double> f1(emat);
    AStarConfSearchFast<double> f2(emat2);
    assertSameScoresSorted(collectAllScores(f1), collectAllScores(f2), 1e-12);
}

TEST(ConfSearchAStar_SYNTHESIZED, Metamorphic_ConstTermShift_ScoreShiftExact) {
    auto emat = makeTinyEmat4pos_2rc();
    auto shifted = emat;
    const double c = 0.25;
    shifted.setConstTerm(shifted.getConstTerm() + c);

    AStarConfSearchBaseline<double> s1(emat);
    AStarConfSearchBaseline<double> s2(shifted);
    auto a = collectAllScores(s1);
    auto b = collectAllScores(s2);
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_LE(std::fabs((a[i] + c) - b[i]), 1e-12);
    }

    AStarConfSearchFast<double> f1(emat);
    AStarConfSearchFast<double> f2(shifted);
    a = collectAllScores(f1);
    b = collectAllScores(f2);
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_LE(std::fabs((a[i] + c) - b[i]), 1e-12);
    }
}

TEST(ConfSearchAStar_SYNTHESIZED, Metamorphic_EnergyLowering_NonIncreasingScores) {
    auto emat = makeTinyEmat4pos_2rc();
    auto lowered = emat;
    // Lower one term; some conformations get strictly lower energies, none increase.
    const double delta = 0.5;
    lowered.setOneBody(1, 0, lowered.getOneBody(1, 0) - delta);

    AStarConfSearchBaseline<double> s1(emat);
    AStarConfSearchBaseline<double> s2(lowered);
    auto a = collectAllScores(s1);
    auto b = collectAllScores(s2);
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_LE(b[i], a[i] + 1e-12);
    }

    AStarConfSearchFast<double> f1(emat);
    AStarConfSearchFast<double> f2(lowered);
    a = collectAllScores(f1);
    b = collectAllScores(f2);
    std::sort(a.begin(), a.end());
    std::sort(b.begin(), b.end());
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_LE(b[i], a[i] + 1e-12);
    }
}

TEST(ConfSearchAStar_SYNTHESIZED, Metamorphic_TieHeavy_DeterminismOrderInsensitive) {
    const auto emat = makeTieHeavyEmat4pos_2rc();

    AStarConfSearchBaseline<double> b1(emat);
    AStarConfSearchBaseline<double> b2(emat);
    assertSameScoresSorted(collectAllScores(b1), collectAllScores(b2), 0.0);

    AStarConfSearchFast<double> f1(emat);
    AStarConfSearchFast<double> f2(emat);
    assertSameScoresSorted(collectAllScores(f1), collectAllScores(f2), 0.0);

    // Baseline vs fast: same score multiset.
    AStarConfSearchBaseline<double> b3(emat);
    AStarConfSearchFast<double> f3(emat);
    assertSameScoresSorted(collectAllScores(b3), collectAllScores(f3), 0.0);
}
