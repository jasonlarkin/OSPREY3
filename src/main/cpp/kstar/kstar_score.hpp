#pragma once

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace osprey::kstar {

// Minimal subset of Java PartitionFunction.Status needed for TestKStarScore.
enum class PfuncStatus : std::uint8_t {
    Estimating,
    Estimated,
    Unstable
};

// Minimal subset of Java PartitionFunction.Values/Result needed for TestKStarScore.
// In Java:
// - lowerBound = qstar
// - upperBound = qstar + qprime (or +infinity)
struct PfuncResult final {
    PfuncStatus status = PfuncStatus::Estimating;
    double qstar = 0.0;                      // lower bound
    double qprime = 0.0;                     // upper-lower (can be +inf)

    [[nodiscard]] double lowerBound() const noexcept { return qstar; }
    [[nodiscard]] double upperBound() const noexcept { return qstar + qprime; }
};

// Port of edu.duke.cs.osprey.kstar.KStarScore (ONLY the math semantics exercised by TestKStarScore).
struct KStarScore final {

    // null in Java; explicitly initialize to avoid compiler "maybe-uninitialized" false positives
    std::optional<double> score = std::nullopt;
    double lowerBound = std::numeric_limits<double>::quiet_NaN();
    double upperBound = std::numeric_limits<double>::quiet_NaN();

    KStarScore() = default;

    KStarScore(const PfuncResult& protein, const PfuncResult& ligand, const PfuncResult& complex) noexcept {

        // Java: score only if all are Estimated, and NaN -> null
        if (protein.status == PfuncStatus::Estimated
            && ligand.status == PfuncStatus::Estimated
            && complex.status == PfuncStatus::Estimated) {

            const double x = complex.qstar / (protein.qstar * ligand.qstar);
            if (std::isnan(x)) {
                score = std::nullopt;
            } else {
                score = x;
            }
        } else {
            score = std::nullopt;
        }

        // Java lowerBound = complex.lower / (protein.upper * ligand.upper)
        lowerBound = complex.lowerBound() / (protein.upperBound() * ligand.upperBound());

        // Java upperBound = complex.upper / (protein.lower * ligand.lower)
        upperBound = complex.upperBound() / (protein.lowerBound() * ligand.lowerBound());
    }
};

} // namespace osprey::kstar


