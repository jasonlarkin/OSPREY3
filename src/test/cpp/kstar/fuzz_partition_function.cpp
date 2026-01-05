// Fuzz target: PartitionFunction on tiny synthetic EnergyMatrix instances.
//
// Goals:
// - Exercise PartitionFunction bounds logic across A* (baseline/fast) and GradientDescent methods.
// - Stress edge conditions (epsilon, tie-heavy energies, extreme magnitudes).
// - Enforce determinism: same input/options => same result.
// - Enforce non-finite policy: NaN anywhere in EnergyMatrix must yield a non-converged NaN result.

#include "partition_function.hpp"
#include "energy_matrix.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace {

struct Cursor final {
    const std::uint8_t* p = nullptr;
    std::size_t n = 0;
    std::size_t i = 0;

    [[nodiscard]] bool has(std::size_t k) const noexcept { return i + k <= n; }

    [[nodiscard]] std::uint8_t u8() noexcept {
        if (!has(1)) return 0;
        return p[i++];
    }

    [[nodiscard]] std::int8_t i8() noexcept {
        return static_cast<std::int8_t>(u8());
    }
};

static double scale_i8(std::int8_t v, double denom) noexcept {
    return static_cast<double>(v) / denom;
}

static void trap_if(bool cond) {
    if (cond) __builtin_trap();
}

static bool approx_eq(double a, double b, double tol) noexcept {
    if (std::isnan(a) || std::isnan(b)) return false;
    return std::fabs(a - b) <= tol;
}

template<typename T>
static bool approx_eq_t(T a, T b, T tol) noexcept {
    if (std::isnan(a) || std::isnan(b)) return false;
    return std::fabs(a - b) <= tol;
}

static void run_one(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 8) return;

    Cursor c{data, size, 0};

    // Keep spaces tiny; oracle and determinism checks are O(N) in number of conformations.
    const int32_t num_positions = 2 + static_cast<int32_t>(c.u8() % 7); // 2..8

    // Config bits:
    // - bit0: prefer fast A* variant (when method==AStar)
    // - bit1: use GradientDescent method (else AStar)
    // - bit2: epsilon mode (0 => epsilon=0, 1 => epsilon ~ 0.2)
    // - bit3: extreme energies (larger magnitude)
    // - bit4: tie-heavy energies (many equal values)
    // - bit5: inject non-finite (NaN) into the matrix (tests NaN policy)
    // - bit6: oracle containment check for epsilon>0 (A* only)
    // - bit7: run float instantiation (EnergyMatrix<float> + PartitionFunction<float>) to exercise template coverage
    const std::uint8_t cfg = c.u8();
    const bool prefer_fast = (cfg & 0x01) != 0;
    const bool use_gd = (cfg & 0x02) != 0;
    const bool eps_zero = (cfg & 0x04) != 0;
    const bool extreme = (cfg & 0x08) != 0;
    const bool tie_heavy = (cfg & 0x10) != 0;
    const bool inject_nan = (cfg & 0x20) != 0;
    const bool oracle_check = (cfg & 0x40) != 0;
    const bool use_float = (cfg & 0x80) != 0;

    std::vector<int32_t> num_confs_per_pos(static_cast<std::size_t>(num_positions), 1);
    std::uint64_t total_confs = 1;
    std::uint64_t total_pairwise_terms = 0;
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        const int32_t nconf = 1 + static_cast<int32_t>(c.u8() % 6); // 1..6
        num_confs_per_pos[static_cast<std::size_t>(pos)] = nconf;
        total_confs *= static_cast<std::uint64_t>(nconf);
        if (total_confs > 20000) return;
    }
    for (int32_t pos1 = 1; pos1 < num_positions; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            total_pairwise_terms += static_cast<std::uint64_t>(num_confs_per_pos[static_cast<std::size_t>(pos1)]) *
                                   static_cast<std::uint64_t>(num_confs_per_pos[static_cast<std::size_t>(pos2)]);
            if (total_pairwise_terms > 120000) return;
        }
    }

    // Keep the fuzzer fast: the baseline A* variant is intentionally allocation-heavy and can be very slow.
    // We still allow it, but only for very small spaces. Fast A* and GD are allowed on larger (still bounded) spaces.
    if (!use_gd && !prefer_fast) {
        if (num_positions > 6) return;
        if (total_confs > 512) return;
    }

    // If epsilon=0 and exact enumeration is disabled, A* will fully enumerate; keep it small.
    if (!use_gd && eps_zero && total_confs > 2048) return;

    // Gradient descent uses a ConfSearch internally; in this port it can be very allocation-heavy
    // (especially when backed by the baseline A* ConfSearch). Keep it tightly bounded.
    if (use_gd) {
        if (num_positions > 6) return;
        if (total_confs > 1024) return;
    }

    const auto method = use_gd ? osprey::kstar::PartitionFunctionMethod::GradientDescent
                               : osprey::kstar::PartitionFunctionMethod::AStar;

    if (!use_float) {
        osprey::kstar::EnergyMatrix<double> emat(num_positions, num_confs_per_pos);

        const double denom_const = extreme ? 1.0 : 16.0;
        const double denom_one = extreme ? 1.0 : 8.0;
        const double denom_pair = extreme ? 1.0 : 16.0;

        emat.setConstTerm(scale_i8(c.i8(), denom_const));

        for (int32_t pos = 0; pos < num_positions; ++pos) {
            const int32_t nconf = num_confs_per_pos[static_cast<std::size_t>(pos)];
            for (int32_t rc = 0; rc < nconf; ++rc) {
                const double v = tie_heavy ? 0.0 : scale_i8(c.i8(), denom_one);
                emat.setOneBody(pos, rc, v);
            }
        }
        for (int32_t pos1 = 1; pos1 < num_positions; ++pos1) {
            const int32_t n1 = num_confs_per_pos[static_cast<std::size_t>(pos1)];
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                const int32_t n2 = num_confs_per_pos[static_cast<std::size_t>(pos2)];
                for (int32_t rc1 = 0; rc1 < n1; ++rc1) {
                    for (int32_t rc2 = 0; rc2 < n2; ++rc2) {
                        const double v = tie_heavy ? 0.0 : scale_i8(c.i8(), denom_pair);
                        emat.setPairwise(pos1, rc1, pos2, rc2, v);
                    }
                }
            }
        }

        const bool has_nan = inject_nan;
        if (inject_nan) {
            emat.setOneBody(0, 0, std::numeric_limits<double>::quiet_NaN());
        }

        osprey::kstar::PartitionFunction<double> pfunc;
        osprey::kstar::PartitionFunction<double>::ComputeOptions opts;
        opts.allow_exact_enumeration = false;
        opts.astar_variant = use_gd
            ? osprey::kstar::AStarVariant::Fast
            : (prefer_fast ? osprey::kstar::AStarVariant::Fast : osprey::kstar::AStarVariant::Baseline);

        const double epsilon = eps_zero ? 0.0 : 0.2;

        const auto r1 = pfunc.compute(emat, epsilon, method, opts);
        const auto r2 = pfunc.compute(emat, epsilon, method, opts);

        trap_if(r1.converged != r2.converged);
        trap_if(r1.num_confs != r2.num_confs);
        trap_if(!approx_eq(r1.delta, r2.delta, 0.0));
        if (std::isnan(r1.lower_bound) || std::isnan(r2.lower_bound)) {
            trap_if(!(std::isnan(r1.lower_bound) && std::isnan(r2.lower_bound)));
        } else {
            trap_if(!approx_eq(r1.lower_bound, r2.lower_bound, 0.0));
        }
        if (std::isnan(r1.upper_bound) || std::isnan(r2.upper_bound)) {
            trap_if(!(std::isnan(r1.upper_bound) && std::isnan(r2.upper_bound)));
        } else {
            trap_if(!approx_eq(r1.upper_bound, r2.upper_bound, 0.0));
        }

        if (has_nan) {
            trap_if(r1.converged);
            trap_if(r1.num_confs != 0);
            trap_if(!std::isnan(r1.lower_bound) || !std::isnan(r1.upper_bound));
            return;
        }

        trap_if(!(r1.lower_bound <= r1.upper_bound || (std::isnan(r1.lower_bound) || std::isnan(r1.upper_bound))));

        if (!use_gd && oracle_check && epsilon > 0.0 && total_confs <= 4096) {
            osprey::kstar::PartitionFunction<double>::ComputeOptions o;
            o.allow_exact_enumeration = true;
            o.astar_variant = osprey::kstar::AStarVariant::Baseline;
            const auto oracle = pfunc.compute(emat, 0.0, osprey::kstar::PartitionFunctionMethod::AStar, o);
            trap_if(!oracle.converged);
            trap_if(!(oracle.lower_bound == oracle.upper_bound));
            trap_if(r1.lower_bound > oracle.lower_bound + 1e-9);
            trap_if(r1.upper_bound + 1e-9 < oracle.lower_bound);
        }
    } else {
        // Float instantiation path (coverage ROI: exercise float template instantiations).
        osprey::kstar::EnergyMatrix<float> emat(num_positions, num_confs_per_pos);

        const float denom_const = extreme ? 1.0f : 16.0f;
        const float denom_one = extreme ? 1.0f : 8.0f;
        const float denom_pair = extreme ? 1.0f : 16.0f;

        emat.setConstTerm(static_cast<float>(scale_i8(c.i8(), static_cast<double>(denom_const))));

        for (int32_t pos = 0; pos < num_positions; ++pos) {
            const int32_t nconf = num_confs_per_pos[static_cast<std::size_t>(pos)];
            for (int32_t rc = 0; rc < nconf; ++rc) {
                const float v = tie_heavy ? 0.0f : static_cast<float>(scale_i8(c.i8(), static_cast<double>(denom_one)));
                emat.setOneBody(pos, rc, v);
            }
        }
        for (int32_t pos1 = 1; pos1 < num_positions; ++pos1) {
            const int32_t n1 = num_confs_per_pos[static_cast<std::size_t>(pos1)];
            for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                const int32_t n2 = num_confs_per_pos[static_cast<std::size_t>(pos2)];
                for (int32_t rc1 = 0; rc1 < n1; ++rc1) {
                    for (int32_t rc2 = 0; rc2 < n2; ++rc2) {
                        const float v = tie_heavy ? 0.0f : static_cast<float>(scale_i8(c.i8(), static_cast<double>(denom_pair)));
                        emat.setPairwise(pos1, rc1, pos2, rc2, v);
                    }
                }
            }
        }

        const bool has_nan = inject_nan;
        if (inject_nan) {
            emat.setOneBody(0, 0, std::numeric_limits<float>::quiet_NaN());
        }

        osprey::kstar::PartitionFunction<float> pfunc;
        osprey::kstar::PartitionFunction<float>::ComputeOptions opts;
        opts.allow_exact_enumeration = false;
        opts.astar_variant = use_gd
            ? osprey::kstar::AStarVariant::Fast
            : (prefer_fast ? osprey::kstar::AStarVariant::Fast : osprey::kstar::AStarVariant::Baseline);

        const float epsilon = eps_zero ? 0.0f : 0.2f;

        const auto r1 = pfunc.compute(emat, epsilon, method, opts);
        const auto r2 = pfunc.compute(emat, epsilon, method, opts);

        trap_if(r1.converged != r2.converged);
        trap_if(r1.num_confs != r2.num_confs);
        trap_if(!approx_eq_t(r1.delta, r2.delta, 0.0f));
        if (std::isnan(r1.lower_bound) || std::isnan(r2.lower_bound)) {
            trap_if(!(std::isnan(r1.lower_bound) && std::isnan(r2.lower_bound)));
        } else {
            trap_if(!approx_eq_t(r1.lower_bound, r2.lower_bound, 0.0f));
        }
        if (std::isnan(r1.upper_bound) || std::isnan(r2.upper_bound)) {
            trap_if(!(std::isnan(r1.upper_bound) && std::isnan(r2.upper_bound)));
        } else {
            trap_if(!approx_eq_t(r1.upper_bound, r2.upper_bound, 0.0f));
        }

        if (has_nan) {
            trap_if(r1.converged);
            trap_if(r1.num_confs != 0);
            trap_if(!std::isnan(r1.lower_bound) || !std::isnan(r1.upper_bound));
            return;
        }

        trap_if(!(r1.lower_bound <= r1.upper_bound));

        if (!use_gd && oracle_check && epsilon > 0.0f && total_confs <= 4096) {
            osprey::kstar::PartitionFunction<float>::ComputeOptions o;
            o.allow_exact_enumeration = true;
            o.astar_variant = osprey::kstar::AStarVariant::Baseline;
            const auto oracle = pfunc.compute(emat, 0.0f, osprey::kstar::PartitionFunctionMethod::AStar, o);
            trap_if(!oracle.converged);
            trap_if(!(oracle.lower_bound == oracle.upper_bound));
            trap_if(r1.lower_bound > oracle.lower_bound + 1e-4f);
            trap_if(r1.upper_bound + 1e-4f < oracle.lower_bound);
        }
    }
}

} // namespace

// libFuzzer entrypoint.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        run_one(data, size);
    } catch (...) {
        // ignore
    }
    return 0;
}

