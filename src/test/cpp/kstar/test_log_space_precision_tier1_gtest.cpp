#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

#include "log_space.hpp"

using osprey::kstar::logspace::log10Add;
using osprey::kstar::logspace::log10Sub;

static bool isFinite(double x) {
    return std::isfinite(x) && !std::isnan(x);
}

TEST(LogSpace_PRECISION_Tier1, NaNPropagation) {
    const double qnan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_TRUE(std::isnan(log10Add(qnan, 1.0)));
    EXPECT_TRUE(std::isnan(log10Add(1.0, qnan)));
    EXPECT_TRUE(std::isnan(log10Sub(qnan, 1.0)));
    EXPECT_TRUE(std::isnan(log10Sub(1.0, qnan)));
}

TEST(LogSpace_PRECISION_Tier1, Add_BoundsAndMonotonicity_Random) {
    // Deterministic seed for reproducibility.
    std::mt19937_64 rng(0x5150ULL);
    std::uniform_real_distribution<double> dist(-50.0, 50.0);

    constexpr int cases = 2000;
    constexpr double log10_2 = 0.3010299956639812; // log10(2)

    for (int i = 0; i < cases; ++i) {
        const double a = dist(rng);
        const double b = dist(rng);

        const double s = log10Add(a, b);
        SCOPED_TRACE(::testing::Message() << "case=" << i << " a=" << a << " b=" << b << " s=" << s);

        ASSERT_TRUE(isFinite(s));

        const double mx = std::max(a, b);
        EXPECT_GE(s, mx);
        // Upper bound: log10(10^a + 10^b) <= max(a,b) + log10(2)
        EXPECT_LE(s, mx + log10_2 + 1e-12);
    }
}

TEST(LogSpace_PRECISION_Tier1, Add_DominanceCutoff_Boundary) {
    // Implementation uses a strict diff > 36 or diff < -36 cutoff.
    const double a = 10.0;
    EXPECT_DOUBLE_EQ(log10Add(a, a - 1000.0), a);
    EXPECT_DOUBLE_EQ(log10Add(a - 1000.0, a), a);

    // Just below the threshold: should be finite and close to max(a,b), but not necessarily exactly equal.
    const double b = a - 35.999;
    const double s = log10Add(a, b);
    EXPECT_TRUE(isFinite(s));
    EXPECT_GE(s, a);
}

TEST(LogSpace_PRECISION_Tier1, Sub_BasicOrderingAndBounds_RandomSafeRange) {
    // Keep values small enough that pow(10, x) stays finite and stable for validation.
    std::mt19937_64 rng(0xC0DEC0DEULL);
    std::uniform_real_distribution<double> dist(-5.0, 5.0);

    constexpr int cases = 2000;
    const double neg_inf = std::numeric_limits<double>::lowest();

    for (int i = 0; i < cases; ++i) {
        double a = dist(rng);
        double b = dist(rng);
        if (b > a) std::swap(a, b);

        // Ensure strict a > b so the subtraction is well-defined in linear space.
        if (a == b) {
            b = std::nextafter(b, -std::numeric_limits<double>::infinity());
        }

        const double s = log10Sub(a, b);
        SCOPED_TRACE(::testing::Message() << "case=" << i << " a=" << a << " b=" << b << " s=" << s);

        ASSERT_TRUE(isFinite(s));
        EXPECT_LE(s, a);

        const double lin = std::pow(10.0, a) - std::pow(10.0, b);
        ASSERT_GT(lin, 0.0);
        const double expected = std::log10(lin);
        EXPECT_NEAR(s, expected, 1e-12);

        // Subtracting 0 (neg_inf) is identity.
        EXPECT_DOUBLE_EQ(log10Sub(a, neg_inf), a);
    }
}

TEST(LogSpace_PRECISION_Tier1, Sub_DominanceCutoff) {
    const double big = 50.0;
    const double tiny = big - 1000.0;
    EXPECT_DOUBLE_EQ(log10Sub(big, tiny), big);
}

