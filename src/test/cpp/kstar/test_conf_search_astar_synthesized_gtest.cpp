#include <gtest/gtest.h>

#include <cstdint>
#include <unordered_set>

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

