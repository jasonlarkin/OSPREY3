#include "energy_matrix.hpp"
#include "partition_function.hpp"
#include "test_data_paths.hpp"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

namespace {

namespace fs = std::filesystem;

std::vector<std::uint8_t> readAllBytes(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    if (!in) {
        throw std::runtime_error("failed to open file: " + p.string());
    }
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size < 0) {
        throw std::runtime_error("invalid size for file: " + p.string());
    }

    std::vector<std::uint8_t> buf(static_cast<std::size_t>(size));
    if (!buf.empty()) {
        in.read(reinterpret_cast<char*>(buf.data()), static_cast<std::streamsize>(buf.size()));
        if (!in) {
            throw std::runtime_error("failed to read file: " + p.string());
        }
    }
    return buf;
}

template<typename T>
static void run_one_bytes_t(const std::uint8_t* data, std::size_t size) {
    // Mirror the fuzzer/corpus-runner byte format for partition_function fuzzing.
    if (size < 3) return;

    const std::uint8_t b0 = data[0];
    const std::uint8_t cfg = data[1];
    const std::uint8_t b2 = data[2];

    const int num_positions = 2 + (b0 % 9); // 2..10
    const bool use_gd = (cfg & 0x01) != 0;
    const bool prefer_fast = (cfg & 0x02) != 0;
    const bool extreme_energies = (cfg & 0x04) != 0;
    const bool tie_heavy = (cfg & 0x10) != 0;
    const bool inject_nan = (cfg & 0x20) != 0;
    const bool oracle_check = (cfg & 0x40) != 0;

    std::vector<int> num_confs_per_pos;
    num_confs_per_pos.reserve(static_cast<std::size_t>(num_positions));

    std::size_t idx = 3;
    for (int p = 0; p < num_positions; ++p) {
        const std::uint8_t bp = (idx < size) ? data[idx++] : 0u;
        num_confs_per_pos.push_back(1 + (bp % 8)); // 1..8
    }

    std::uint64_t total_confs = 1;
    for (int p = 0; p < num_positions; ++p) {
        total_confs *= static_cast<std::uint64_t>(num_confs_per_pos[p]);
        if (total_confs > 4096) {
            // Keep the regression test bounded (avoid pathological blowups).
            return;
        }
    }

    osprey::kstar::EnergyMatrix<T> emat(num_positions, num_confs_per_pos);

    auto next_i8 = [&]() -> std::int8_t {
        if (idx >= size) return 0;
        return static_cast<std::int8_t>(data[idx++]);
    };
    auto next_energy = [&]() -> T {
        std::int8_t v = next_i8();
        if (tie_heavy) v = static_cast<std::int8_t>(v & 0x07);
        const T scale = extreme_energies ? static_cast<T>(5) : static_cast<T>(1);
        return static_cast<T>(v) * scale;
    };

    // const term
    emat.setConstTerm(inject_nan ? std::numeric_limits<T>::quiet_NaN() : next_energy());

    // one-body
    for (int p = 0; p < num_positions; ++p) {
        for (int rc = 0; rc < num_confs_per_pos[p]; ++rc) {
            emat.setOneBody(p, rc, inject_nan ? std::numeric_limits<T>::quiet_NaN() : next_energy());
        }
    }

    // pairwise
    for (int p1 = 0; p1 < num_positions; ++p1) {
        for (int p2 = p1 + 1; p2 < num_positions; ++p2) {
            for (int rc1 = 0; rc1 < num_confs_per_pos[p1]; ++rc1) {
                for (int rc2 = 0; rc2 < num_confs_per_pos[p2]; ++rc2) {
                    emat.setPairwise(p1, rc1, p2, rc2, inject_nan ? std::numeric_limits<T>::quiet_NaN() : next_energy());
                }
            }
        }
    }

    const T epsilon = static_cast<T>((b2 % 100) / static_cast<T>(100)); // 0.00..0.99

    osprey::kstar::PartitionFunction<T> pfunc;

    // Keep GD bounded in the regression test; the fuzzer target is already tuned for this too.
    if (use_gd && (num_positions > 6 || total_confs > 1024)) return;

    typename osprey::kstar::PartitionFunction<T>::ComputeOptions opts;
    opts.astar_variant = prefer_fast ? osprey::kstar::AStarVariant::Fast : osprey::kstar::AStarVariant::Baseline;
    // Force GD's internal A* backend to fast to avoid expensive baseline allocations on larger-ish cases.
    if (use_gd) opts.astar_variant = osprey::kstar::AStarVariant::Fast;

    const auto method = use_gd ? osprey::kstar::PartitionFunctionMethod::GradientDescent
                               : osprey::kstar::PartitionFunctionMethod::AStar;

    const auto r1 = pfunc.compute(emat, epsilon, method, opts);
    const auto r2 = pfunc.compute(emat, epsilon, method, opts);

    // Determinism: for fixed input/options, we should get the same result twice.
    // (Exact equality is expected here because all computations are deterministic for a fixed build.)
    EXPECT_EQ(r1.converged, r2.converged);
    EXPECT_EQ(r1.num_confs, r2.num_confs);
    if (std::isnan(r1.lower_bound) || std::isnan(r2.lower_bound)) {
        EXPECT_TRUE(std::isnan(r1.lower_bound) && std::isnan(r2.lower_bound));
    } else {
        EXPECT_EQ(r1.lower_bound, r2.lower_bound);
    }
    if (std::isnan(r1.upper_bound) || std::isnan(r2.upper_bound)) {
        EXPECT_TRUE(std::isnan(r1.upper_bound) && std::isnan(r2.upper_bound));
    } else {
        EXPECT_EQ(r1.upper_bound, r2.upper_bound);
    }

    if (inject_nan) {
        EXPECT_FALSE(r1.converged);
        EXPECT_EQ(r1.num_confs, 0u);
        EXPECT_TRUE(std::isnan(r1.lower_bound));
        EXPECT_TRUE(std::isnan(r1.upper_bound));
        return;
    }

    EXPECT_LE(r1.lower_bound, r1.upper_bound);

    if (!use_gd && oracle_check && epsilon > static_cast<T>(0) && total_confs <= 4096) {
        typename osprey::kstar::PartitionFunction<T>::ComputeOptions o;
        o.allow_exact_enumeration = true;
        o.astar_variant = osprey::kstar::AStarVariant::Baseline;
        const auto oracle = pfunc.compute(emat, static_cast<T>(0), osprey::kstar::PartitionFunctionMethod::AStar, o);
        EXPECT_TRUE(oracle.converged);
        EXPECT_EQ(oracle.lower_bound, oracle.upper_bound);
        EXPECT_LE(r1.lower_bound, oracle.lower_bound + static_cast<T>(1e-4));
        EXPECT_GE(r1.upper_bound + static_cast<T>(1e-4), oracle.lower_bound);
    }
}

} // namespace

TEST(PartitionFunctionRegression, PromotedFuzzInputsDontCrash) {
    // This regression test intentionally does NOT require committed binary inputs.
    //
    // It always runs a small embedded seed corpus to ensure core invariants remain stable.
    // If you have additional corpus inputs (e.g., from fuzzing), place them under:
    //   build/cpp/kstar/test_data/fuzz/partition_function/
    // and they will be replayed as well.
    std::optional<fs::path> dirOpt = osprey::kstar::testutil::resolveTestDataPath("fuzz/partition_function");

    // Minimal embedded seeds (avoid committing binaries, still protects against regressions).
    const std::vector<std::vector<std::uint8_t>> embeddedSeeds = {
        {}, {0x00}, {0x00, 0x00}, {0x00, 0x00, 0x00},
        std::vector<std::uint8_t>(32, 0x00),
        std::vector<std::uint8_t>(64, 0x00),
        // A small "random-looking" seed to avoid being all-zeros.
        {0x07, 0x42, 0x63, 0x01, 0x02, 0x03, 0xFF, 0x10, 0x80, 0x7F, 0x00, 0x05, 0xAA, 0x55},
    };

    for (const auto& seed : embeddedSeeds) {
        run_one_bytes_t<double>(seed.data(), seed.size());
        run_one_bytes_t<float>(seed.data(), seed.size());
    }

    if (dirOpt && fs::exists(*dirOpt) && fs::is_directory(*dirOpt)) {
        for (const auto& ent : fs::directory_iterator(*dirOpt)) {
            if (!ent.is_regular_file()) continue;
            const fs::path p = ent.path();
            if (p.extension() != ".bin") continue;

            const auto bytes = readAllBytes(p);
            run_one_bytes_t<double>(bytes.data(), bytes.size());
            run_one_bytes_t<float>(bytes.data(), bytes.size());
        }
    }
}

