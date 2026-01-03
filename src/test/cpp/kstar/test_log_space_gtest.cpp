#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "log_space.hpp"

using osprey::kstar::logspace::log10Add;
using osprey::kstar::logspace::log10Sub;

TEST(LogSpace_SYNTHESIZED, Add_IdentityAndSymmetry) {
    const double neg_inf = std::numeric_limits<double>::lowest();

    EXPECT_DOUBLE_EQ(log10Add(neg_inf, 3.0), 3.0);
    EXPECT_DOUBLE_EQ(log10Add(3.0, neg_inf), 3.0);

    const double a = 1.234;
    const double b = -3.21;
    EXPECT_NEAR(log10Add(a, b), log10Add(b, a), 1e-12);
}

TEST(LogSpace_SYNTHESIZED, Add_DominanceCutoff) {
    // If the gap is huge, the smaller term is negligible in double precision.
    const double big = 100.0;
    const double small = big - 1000.0;
    EXPECT_DOUBLE_EQ(log10Add(big, small), big);
    EXPECT_DOUBLE_EQ(log10Add(small, big), big);
}

TEST(LogSpace_SYNTHESIZED, Sub_BasicIdentities) {
    const double neg_inf = std::numeric_limits<double>::lowest();

    // subtracting 0
    EXPECT_DOUBLE_EQ(log10Sub(5.0, neg_inf), 5.0);

    // cancellation -> 0
    EXPECT_DOUBLE_EQ(log10Sub(1.0, 1.0), neg_inf);

    // invalid ordering clamps to 0
    EXPECT_DOUBLE_EQ(log10Sub(1.0, 2.0), neg_inf);
}

TEST(LogSpace_SYNTHESIZED, Sub_DominanceCutoff) {
    const double big = 50.0;
    const double tiny = big - 1000.0;
    EXPECT_DOUBLE_EQ(log10Sub(big, tiny), big);
}

