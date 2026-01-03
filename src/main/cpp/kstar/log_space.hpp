// Log-space arithmetic helpers (base-10).
//
// This is used to keep PartitionFunction numerically stable without BigDecimal/MPFR
// by representing large partition-function sums as log10(Q).

#ifndef OSPREY_KSTAR_LOG_SPACE_HPP
#define OSPREY_KSTAR_LOG_SPACE_HPP

#include <algorithm>
#include <cmath>
#include <concepts>
#include <limits>

namespace osprey {
namespace kstar {
namespace logspace {

template<std::floating_point T>
[[nodiscard]] inline T log10Add(T log_a, T log_b) noexcept {
    // log10(10^a + 10^b) = max(a, b) + log10(1 + 10^(min-max))

    if (std::isnan(log_a) || std::isnan(log_b)) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    const T neg_inf = std::numeric_limits<T>::lowest();

    if (log_a == neg_inf) return log_b;
    if (log_b == neg_inf) return log_a;

    // If one term is far smaller, it won't affect double/float precision.
    const T diff = log_a - log_b;
    if (diff > T(36)) return log_a;
    if (diff < T(-36)) return log_b;

    const T max_val = std::max(log_a, log_b);
    const T min_val = std::min(log_a, log_b);
    const T x = min_val - max_val; // <= 0

    return max_val + std::log10(T(1) + std::pow(T(10), x));
}

template<std::floating_point T>
[[nodiscard]] inline T log10Sub(T log_a, T log_b) noexcept {
    // log10(10^a - 10^b) = a + log10(1 - 10^(b-a)), requires a > b.
    if (std::isnan(log_a) || std::isnan(log_b)) {
        return std::numeric_limits<T>::quiet_NaN();
    }

    const T neg_inf = std::numeric_limits<T>::lowest();

    if (log_b == neg_inf) return log_a;  // subtracting 0
    if (log_a == neg_inf) return neg_inf; // 0 - something => clamp to 0
    if (log_b >= log_a) return neg_inf;   // invalid or exact cancellation -> 0

    const T diff = log_b - log_a; // < 0
    if (diff < T(-36)) return log_a;

    const T x = std::pow(T(10), diff); // in (0,1)
    if (x >= T(1)) return neg_inf;

    return log_a + std::log10(T(1) - x);
}

} // namespace logspace
} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_LOG_SPACE_HPP

