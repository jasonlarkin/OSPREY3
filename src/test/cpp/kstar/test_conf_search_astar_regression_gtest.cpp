#include "conf_search_astar.hpp"
#include "energy_matrix.hpp"
#include "test_data_paths.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
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

static void run_one_bytes(const std::uint8_t* data, std::size_t size) {
    if (!data || size < 8) return;

    Cursor c{data, size, 0};
    const int32_t num_positions = 2 + static_cast<int32_t>(c.u8() % 9); // 2..10

    // Config bits mirror fuzz target:
    // - bit0: prefer fast variant for primary run
    // - bit1: cross-check (baseline vs fast score sets)
    // - bit2: extreme energies
    // - bit3: tie-heavy energies
    const std::uint8_t cfg = c.u8();
    const bool prefer_fast = (cfg & 0x01) != 0;
    const bool cross_check = (cfg & 0x02) != 0;
    const bool extreme = (cfg & 0x04) != 0;
    const bool tie_heavy = (cfg & 0x08) != 0;

    std::vector<int32_t> num_confs_per_pos(static_cast<std::size_t>(num_positions), 1);
    std::uint64_t total_confs = 1;
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        const int32_t nconf = 1 + static_cast<int32_t>(c.u8() % 8); // 1..8
        num_confs_per_pos[static_cast<std::size_t>(pos)] = nconf;
        total_confs *= static_cast<std::uint64_t>(nconf);
        if (total_confs > 20000) return;
    }

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

    const auto primary_variant = prefer_fast ? osprey::kstar::AStarVariant::Fast : osprey::kstar::AStarVariant::Baseline;
    const std::uint64_t max_steps = (total_confs < 2048) ? total_confs : 2048;

    struct Out final {
        double score = 0.0;
        std::vector<int32_t> assignments;
    };

    const auto run_collect_variant = [&](osprey::kstar::AStarVariant variant, std::uint64_t steps_cap) -> std::vector<Out> {
        auto search = osprey::kstar::makeAStarConfSearch<double>(emat, variant);
        std::vector<Out> outs;
        outs.reserve(static_cast<std::size_t>(steps_cap));

        double prev_score = -std::numeric_limits<double>::infinity();
        for (std::uint64_t step = 0; step < steps_cap; ++step) {
            auto next = search->nextConf();
            if (!next.has_value()) break;
            if (next->score + 1e-12 < prev_score) {
                break;
            }
            prev_score = next->score;

            Out out;
            out.score = next->score;
            out.assignments = next->assignments;
            outs.emplace_back(std::move(out));
        }
        return outs;
    };

    // Smoke run for coverage/regression signal.
    (void)run_collect_variant(primary_variant, max_steps);

    // Determinism check (small cap).
    const std::uint64_t det_steps = (max_steps < 16) ? max_steps : 16;
    const auto a = run_collect_variant(primary_variant, det_steps);
    const auto b = run_collect_variant(primary_variant, det_steps);
    ASSERT_EQ(a.size(), b.size());
    for (std::size_t i = 0; i < a.size(); ++i) {
        ASSERT_EQ(a[i].assignments, b[i].assignments);
        ASSERT_LE(std::fabs(a[i].score - b[i].score), 1e-12);
    }

    // Baseline vs fast cross-check on tiny spaces.
    if (cross_check && total_confs <= 4096) {
        const std::uint64_t n = (max_steps < 32) ? max_steps : 32;
        auto aa = run_collect_variant(osprey::kstar::AStarVariant::Baseline, n);
        auto bb = run_collect_variant(osprey::kstar::AStarVariant::Fast, n);

        std::vector<double> sa;
        std::vector<double> sb;
        sa.reserve(aa.size());
        sb.reserve(bb.size());
        for (const auto& o : aa) sa.push_back(o.score);
        for (const auto& o : bb) sb.push_back(o.score);
        std::sort(sa.begin(), sa.end());
        std::sort(sb.begin(), sb.end());

        ASSERT_EQ(sa.size(), sb.size());
        for (std::size_t i = 0; i < sa.size(); ++i) {
            ASSERT_LE(std::fabs(sa[i] - sb[i]), 1e-9);
        }
    }
}

} // namespace

TEST(ConfSearchAStarRegression, PromotedFuzzInputsDontCrash) {
    // This regression test intentionally does NOT require committed binary inputs.
    //
    // It always runs a small embedded seed set. If you have additional corpus inputs (e.g., from fuzzing),
    // place them under:
    //   build/cpp/kstar/test_data/fuzz/conf_search_astar/
    // and they will be replayed as well.
    std::optional<fs::path> dirOpt = osprey::kstar::testutil::resolveTestDataPath("fuzz/conf_search_astar");

    const std::vector<std::vector<std::uint8_t>> embeddedSeeds = {
        {}, {0x00}, {0x00, 0x00}, {0x00, 0x00, 0x00},
        std::vector<std::uint8_t>(32, 0x00),
        std::vector<std::uint8_t>(64, 0x00),
        // Non-zero seed: positions, cfg, conf counts, energies.
        {0x03, 0x03, 0x02, 0x05, 0x02, 0x07, 0x11, 0xEE, 0x10, 0x7F, 0x00, 0x01, 0x02, 0x03, 0xF0, 0x0F},
    };

    for (const auto& seed : embeddedSeeds) {
        run_one_bytes(seed.data(), seed.size());
    }

    if (dirOpt && fs::exists(*dirOpt) && fs::is_directory(*dirOpt)) {
        for (const auto& ent : fs::directory_iterator(*dirOpt)) {
            if (!ent.is_regular_file()) continue;
            const fs::path p = ent.path();
            if (p.extension() != ".bin") continue;
            const auto bytes = readAllBytes(p);
            run_one_bytes(bytes.data(), bytes.size());
        }
    }
}

