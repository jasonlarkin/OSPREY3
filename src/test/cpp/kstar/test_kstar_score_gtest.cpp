#include <cmath>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

#include "kstar_score.hpp"

using namespace osprey::kstar;

// VERBATIM port of: src/test/java/edu/duke/cs/osprey/kstar/TestKStarScore.java
// Notes:
// - Java uses BigDecimal + MathTools.BigPositiveInfinity; this C++ test uses double with +inf.
// - Java's "null" score is represented as std::optional<double>{}.

static PfuncResult makeResult(PfuncStatus status, double min, double max) {
    PfuncResult r;
    r.status = status;
    r.qstar = min;
    if (max == std::numeric_limits<double>::infinity()) {
        r.qprime = std::numeric_limits<double>::infinity();
    } else {
        r.qprime = max - min;
    }
    return r;
}

struct ExpectedScore {
    std::optional<double> score;
    double lower;
    double upper;
};

static void expectRelativelyEqual(const KStarScore& got, const ExpectedScore& exp) {
    constexpr double rel_eps = 1e-8;

    auto check = [&](const char* name, std::optional<double> gotv, std::optional<double> expv) {
        if (!expv.has_value()) {
            EXPECT_FALSE(gotv.has_value()) << name;
            return;
        }
        ASSERT_TRUE(gotv.has_value()) << name;
        if (std::isnan(*expv)) {
            EXPECT_TRUE(std::isnan(*gotv)) << name;
            return;
        }
        if (std::isinf(*expv)) {
            EXPECT_TRUE(std::isinf(*gotv)) << name;
            EXPECT_EQ(std::signbit(*gotv), std::signbit(*expv)) << name;
            return;
        }
        const double denom = std::max(std::abs(*expv), 1.0);
        EXPECT_LE(std::abs(*gotv - *expv) / denom, rel_eps) << name;
    };

    auto checkD = [&](const char* name, double gotv, double expv) {
        if (std::isnan(expv)) {
            EXPECT_TRUE(std::isnan(gotv)) << name;
            return;
        }
        if (std::isinf(expv)) {
            EXPECT_TRUE(std::isinf(gotv)) << name;
            EXPECT_EQ(std::signbit(gotv), std::signbit(expv)) << name;
            return;
        }
        const double denom = std::max(std::abs(expv), 1.0);
        EXPECT_LE(std::abs(gotv - expv) / denom, rel_eps) << name;
    };

    check("score", got.score, exp.score);
    checkD("lowerBound", got.lowerBound, exp.lower);
    checkD("upperBound", got.upperBound, exp.upper);
}

TEST(KStarScore_VERBATIM, ResultFactory) {
    auto result = makeResult(PfuncStatus::Estimated, 5, 9);
    EXPECT_DOUBLE_EQ(result.qstar, 5.0);
    EXPECT_DOUBLE_EQ(result.qprime, 4.0);
    EXPECT_DOUBLE_EQ(result.lowerBound(), 5.0);
    EXPECT_DOUBLE_EQ(result.upperBound(), 9.0);
}

// stability tests

TEST(KStarScore_VERBATIM, AllStable_PLC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 4.0 / 7 / 6, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, OnlyProteinUnstable_pLC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, OnlyLigandUnstable_PlC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, OnlyComplexUnstable_PLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0)
    );
    expectRelativelyEqual(s, ExpectedScore{0.0, 0.0, 0.0});
}

TEST(KStarScore_VERBATIM, ProteinAndLigandUnstable_plC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ProteinAndComplexUnstable_pLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN()});
}

TEST(KStarScore_VERBATIM, LigandAndComplexUnstable_Plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN()});
}

TEST(KStarScore_VERBATIM, AllUnstable_plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0),
        makeResult(PfuncStatus::Estimated, 0.0, 0.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt,
        std::numeric_limits<double>::quiet_NaN(),
        std::numeric_limits<double>::quiet_NaN()});
}

// lower bound tests

TEST(KStarScore_VERBATIM, ProteinZeroLower_pLC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(),
        4.0 / 7 / 6,
        std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, LigandZeroLower_PlC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(),
        4.0 / 7 / 6,
        std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ComplexZeroLower_PLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{0.0, 0.0, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, ProteinAndLigandZeroLower_plC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(),
        4.0 / 7 / 6,
        std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ProteinAndComplexZeroLower_pLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, LigandAndComplexZeroLower_Plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, AllZeroLower_plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 0.0, std::numeric_limits<double>::infinity()});
}

// upper bound tests

TEST(KStarScore_VERBATIM, ProteinInfUpper_pLC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 0.0, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, LigandInfUpper_PlC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 0.0, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, ComplexInfUpper_PLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 4.0 / 7 / 6, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ProteinAndLigandInfUpper_plC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 3.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 0.0, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, ProteinAndComplexInfUpper_pLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, LigandAndComplexInfUpper_Plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 4.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, AllInfUpper_plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 3.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 4.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{4.0 / 3 / 2, 0.0, std::numeric_limits<double>::infinity()});
}

// lower and upper bound tests

TEST(KStarScore_VERBATIM, ProteinZeroLowerInfUpper_pLC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(), 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, LigandZeroLowerInfUpper_PlC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(), 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ComplexZeroLowerInfUpper_PLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{0.0, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ProteinAndLigandZeroLowerInfUpper_plC) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::numeric_limits<double>::infinity(), 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, ProteinAndComplexZeroLowerInfUpper_pLc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, LigandAndComplexZeroLowerInfUpper_Plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 0.0, std::numeric_limits<double>::infinity()});
}

TEST(KStarScore_VERBATIM, AllZeroLowerInfUpper_plc) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity()),
        makeResult(PfuncStatus::Estimated, 0.0, std::numeric_limits<double>::infinity())
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 0.0, std::numeric_limits<double>::infinity()});
}

// not estimated tests

TEST(KStarScore_VERBATIM, ProteinNotEstimated) {
    KStarScore s(
        makeResult(PfuncStatus::Estimating, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 4.0 / 7 / 6, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, LigandNotEstimated) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimating, 3.0, 7.0),
        makeResult(PfuncStatus::Estimated, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 4.0 / 7 / 6, 8.0 / 3 / 2});
}

TEST(KStarScore_VERBATIM, ComplexNotEstimated) {
    KStarScore s(
        makeResult(PfuncStatus::Estimated, 2.0, 6.0),
        makeResult(PfuncStatus::Estimated, 3.0, 7.0),
        makeResult(PfuncStatus::Estimating, 4.0, 8.0)
    );
    expectRelativelyEqual(s, ExpectedScore{std::nullopt, 4.0 / 7 / 6, 8.0 / 3 / 2});
}


