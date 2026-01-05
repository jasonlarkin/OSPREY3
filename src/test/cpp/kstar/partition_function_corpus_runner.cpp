// Corpus replay runner for PartitionFunction fuzzing.
//
// This is used by coverage builds: it replays any inputs in the fuzz corpus directory so those
// executions contribute to gcov/lcov coverage measurement.
//
// Strict mode:
//   KSTAR_CORPUS_RUNNER_STRICT=1
//     - determinism / invariant violations exit(1) with a message
// Default:
//     - treat violations as "bad input" and skip (non-fatal for coverage runs)

#include "partition_function.hpp"
#include "energy_matrix.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct Cursor final {
    const std::uint8_t* p = nullptr;
    std::size_t n = 0;
    std::size_t i = 0;
    [[nodiscard]] bool has(std::size_t k) const noexcept { return i + k <= n; }
    [[nodiscard]] std::uint8_t u8() noexcept { return has(1) ? p[i++] : 0; }
    [[nodiscard]] std::int8_t i8() noexcept { return static_cast<std::int8_t>(u8()); }
};

static double scale_i8(std::int8_t v, double denom) noexcept {
    return static_cast<double>(v) / denom;
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

static void run_one_bytes(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 8) return;

    const bool strict = (std::getenv("KSTAR_CORPUS_RUNNER_STRICT") != nullptr);
    auto fail = [&](const char* why) {
        if (strict) {
            std::cerr << "[partition_function_corpus_runner] FAIL: " << why << "\n";
            std::exit(1);
        }
    };

    Cursor c{data, size, 0};
    const int32_t num_positions = 2 + static_cast<int32_t>(c.u8() % 7); // 2..8

    const std::uint8_t cfg = c.u8();
    const bool prefer_fast = (cfg & 0x01) != 0;
    const bool use_gd = (cfg & 0x02) != 0;
    const bool eps_zero = (cfg & 0x04) != 0;
    const bool extreme = (cfg & 0x08) != 0;
    const bool tie_heavy = (cfg & 0x10) != 0;
    const bool inject_nan = (cfg & 0x20) != 0;
    const bool oracle_check = (cfg & 0x40) != 0;

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
    // Mirror fuzzer harness bounding to keep coverage runs predictable.
    if (!use_gd && !prefer_fast) {
        if (num_positions > 6) return;
        if (total_confs > 512) return;
    }
    if (!use_gd && eps_zero && total_confs > 2048) return;
    if (use_gd) {
        if (num_positions > 6) return;
        if (total_confs > 1024) return;
    }

    const auto method = use_gd ? osprey::kstar::PartitionFunctionMethod::GradientDescent
                               : osprey::kstar::PartitionFunctionMethod::AStar;

    // Always execute the double path (primary).
    {
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

        if (r1.converged != r2.converged) return fail("determinism: converged mismatch");
        if (r1.num_confs != r2.num_confs) return fail("determinism: num_confs mismatch");
        if (!approx_eq(r1.delta, r2.delta, 0.0)) return fail("determinism: delta mismatch");
        if (std::isnan(r1.lower_bound) || std::isnan(r2.lower_bound)) {
            if (!(std::isnan(r1.lower_bound) && std::isnan(r2.lower_bound))) return fail("determinism: lower NaN mismatch");
        } else {
            if (!approx_eq(r1.lower_bound, r2.lower_bound, 0.0)) return fail("determinism: lower mismatch");
        }
        if (std::isnan(r1.upper_bound) || std::isnan(r2.upper_bound)) {
            if (!(std::isnan(r1.upper_bound) && std::isnan(r2.upper_bound))) return fail("determinism: upper NaN mismatch");
        } else {
            if (!approx_eq(r1.upper_bound, r2.upper_bound, 0.0)) return fail("determinism: upper mismatch");
        }

        if (inject_nan) {
            if (r1.converged) return fail("nan policy: converged unexpectedly");
            if (r1.num_confs != 0) return fail("nan policy: num_confs != 0");
            if (!std::isnan(r1.lower_bound) || !std::isnan(r1.upper_bound)) return fail("nan policy: bounds not NaN");
            return;
        }

        if (!(r1.lower_bound <= r1.upper_bound)) return fail("bounds: lower > upper");

        if (!use_gd && oracle_check && epsilon > 0.0 && total_confs <= 4096) {
            osprey::kstar::PartitionFunction<double>::ComputeOptions o;
            o.allow_exact_enumeration = true;
            o.astar_variant = osprey::kstar::AStarVariant::Baseline;
            const auto oracle = pfunc.compute(emat, 0.0, osprey::kstar::PartitionFunctionMethod::AStar, o);
            if (!oracle.converged) return fail("oracle: did not converge");
            if (!(oracle.lower_bound == oracle.upper_bound)) return fail("oracle: not exact");
            if (r1.lower_bound > oracle.lower_bound + 1e-9) return fail("oracle containment: lower too high");
            if (r1.upper_bound + 1e-9 < oracle.lower_bound) return fail("oracle containment: upper too low");
        }
    }

    // Also execute the float instantiation to exercise template coverage (and catch float-specific issues).
    // This is intentionally unconditional in the corpus runner so fuzz-corpus replay buys stable coverage.
    {
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

        if (r1.converged != r2.converged) return fail("determinism: converged mismatch");
        if (r1.num_confs != r2.num_confs) return fail("determinism: num_confs mismatch");
        if (!approx_eq_t(r1.delta, r2.delta, 0.0f)) return fail("determinism: delta mismatch");
        if (std::isnan(r1.lower_bound) || std::isnan(r2.lower_bound)) {
            if (!(std::isnan(r1.lower_bound) && std::isnan(r2.lower_bound))) return fail("determinism: lower NaN mismatch");
        } else {
            if (!approx_eq_t(r1.lower_bound, r2.lower_bound, 0.0f)) return fail("determinism: lower mismatch");
        }
        if (std::isnan(r1.upper_bound) || std::isnan(r2.upper_bound)) {
            if (!(std::isnan(r1.upper_bound) && std::isnan(r2.upper_bound))) return fail("determinism: upper NaN mismatch");
        } else {
            if (!approx_eq_t(r1.upper_bound, r2.upper_bound, 0.0f)) return fail("determinism: upper mismatch");
        }

        if (inject_nan) {
            if (r1.converged) return fail("nan policy: converged unexpectedly");
            if (r1.num_confs != 0) return fail("nan policy: num_confs != 0");
            if (!std::isnan(r1.lower_bound) || !std::isnan(r1.upper_bound)) return fail("nan policy: bounds not NaN");
            return;
        }

        if (!(r1.lower_bound <= r1.upper_bound)) return fail("bounds: lower > upper");

        if (!use_gd && oracle_check && epsilon > 0.0f && total_confs <= 4096) {
            osprey::kstar::PartitionFunction<float>::ComputeOptions o;
            o.allow_exact_enumeration = true;
            o.astar_variant = osprey::kstar::AStarVariant::Baseline;
            const auto oracle = pfunc.compute(emat, 0.0f, osprey::kstar::PartitionFunctionMethod::AStar, o);
            if (!oracle.converged) return fail("oracle: did not converge");
            if (!(oracle.lower_bound == oracle.upper_bound)) return fail("oracle: not exact");
            if (r1.lower_bound > oracle.lower_bound + 1e-4f) return fail("oracle containment: lower too high");
            if (r1.upper_bound + 1e-4f < oracle.lower_bound) return fail("oracle containment: upper too low");
        }
    }
}

static std::string getCorpusDirDefault() {
    return std::string(OSPREY_REPO_ROOT) + "/build/cpp/kstar-fuzz/fuzz-corpus/partition_function";
}

} // namespace

int main() {
    const char* env = std::getenv("KSTAR_FUZZ_CORPUS_DIR_PARTITION_FUNCTION");
    const std::string corpusDir = (env && *env) ? std::string(env) : getCorpusDirDefault();

    std::error_code ec;
    if (!fs::exists(corpusDir, ec) || !fs::is_directory(corpusDir, ec)) {
        std::cout << "[partition_function_corpus_runner] corpus dir not found; skipping: " << corpusDir << "\n";
        return 0;
    }

    std::size_t filesVisited = 0;
    std::size_t executed = 0;
    std::size_t readErr = 0;

    for (const auto& entry : fs::directory_iterator(corpusDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        ++filesVisited;

        const auto path = entry.path();
        const auto fsize = entry.file_size(ec);
        if (ec) continue;
        if (fsize == 0 || fsize > 4 * 1024 * 1024) continue;

        std::ifstream in(path, std::ios::binary);
        if (!in) {
            ++readErr;
            continue;
        }
        std::vector<std::uint8_t> buf(static_cast<std::size_t>(fsize));
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (!in) {
            ++readErr;
            continue;
        }

        try {
            run_one_bytes(buf.data(), buf.size());
            ++executed;
        } catch (...) {
            // ignore
        }
    }

    std::cout << "[partition_function_corpus_runner] corpusDir=" << corpusDir
              << " filesVisited=" << filesVisited
              << " executed=" << executed
              << " readErr=" << readErr << "\n";
    return 0;
}

