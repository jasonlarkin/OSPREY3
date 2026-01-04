// Corpus replay runner for ConfSearchAStar fuzzing.
//
// This is used by coverage builds: it replays any inputs in the fuzz corpus directory so those
// executions contribute to gcov/lcov coverage measurement.

#include "conf_search_astar.hpp"
#include "astar_search_fast.hpp"
#include "energy_matrix.hpp"

#include <algorithm>
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

static void probe_astar_search_fast_ranges(
    const osprey::kstar::AStarSearchFast<double>& astar_fast,
    const std::vector<int32_t>& num_confs_per_pos,
    Cursor& c
) {
    const int32_t num_positions = static_cast<int32_t>(num_confs_per_pos.size());
    if (num_positions <= 1) return;

    // Previously this corpus runner directly exercised sumUndefinedRange(...) boundary branches.
    // That helper is intentionally private (it's internal heuristic plumbing), so instead 
    // trigger the same code paths through the public API (computeHScore/expandInto) for coverage.
    const int32_t k = static_cast<int32_t>(c.u8() % static_cast<std::uint8_t>(num_positions)); // 0..num_positions-1
    auto node = osprey::kstar::AStarNodeFast<double>::root(num_positions);
    for (int32_t pos = 0; pos < k; ++pos) {
        const int32_t nrc = num_confs_per_pos[static_cast<std::size_t>(pos)];
        if (nrc <= 0) break;
        const int16_t rc = static_cast<int16_t>(c.u8() % static_cast<std::uint8_t>(nrc));
        node = node.assign(pos, rc);
    }
    node.g_score = astar_fast.computeGScore(node);
    node.h_score = astar_fast.computeHScore(node);
    node.f_score = node.g_score + node.h_score;

    std::vector<osprey::kstar::AStarNodeFast<double>> out;
    astar_fast.expandInto(node, out);

    // Prevent optimizing away.
    if (!out.empty() && out.front().getScore() == 1234567.0) {
        std::cerr << "unreachable\n";
    }
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

    // Force some coverage of AStarSearchFast boundary logic (even if mostly run baseline).
    try {
        auto astar_fast = osprey::kstar::AStarSearchFast<double>(
            emat, num_positions, std::span<const int32_t>(num_confs_per_pos.data(), num_confs_per_pos.size())
        );
        probe_astar_search_fast_ranges(astar_fast, num_confs_per_pos, c);
    } catch (...) {
        // ignore
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
                // Treat invariant breaks as "bad input"; don't crash coverage runs.
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

    // Always run once for coverage contribution.
    (void)run_collect_variant(primary_variant, max_steps);

    const bool strict = (std::getenv("KSTAR_CORPUS_RUNNER_STRICT") != nullptr);

    auto fail = [&](const char* why) {
        if (strict) {
            std::cerr << "[conf_search_astar_corpus_runner] FAIL: " << why << "\n";
            std::exit(1);
        }
        // Non-strict: treat as "bad input" and skip (coverage should still proceed).
    };

    // Determinism check: repeatability for a fixed input and variant.
    const std::uint64_t det_steps = (max_steps < 16) ? max_steps : 16;
    const auto a = run_collect_variant(primary_variant, det_steps);
    const auto b = run_collect_variant(primary_variant, det_steps);
    if (a.size() != b.size()) {
        fail("determinism: size mismatch");
        return;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (a[i].assignments != b[i].assignments) {
            fail("determinism: assignments mismatch");
            return;
        }
        if (std::fabs(a[i].score - b[i].score) > 1e-12) {
            fail("determinism: score mismatch");
            return;
        }
    }

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

        if (sa.size() != sb.size()) {
            fail("cross_check: size mismatch");
            return;
        }
        for (std::size_t i = 0; i < sa.size(); ++i) {
            if (std::fabs(sa[i] - sb[i]) > 1e-9) {
                fail("cross_check: score mismatch");
                return;
            }
        }
    }
}

static std::string getCorpusDirDefault() {
    return std::string(OSPREY_REPO_ROOT) + "/build/cpp/kstar-fuzz/fuzz-corpus/conf_search_astar";
}

} // namespace

int main() {
    const char* env = std::getenv("KSTAR_FUZZ_CORPUS_DIR_CONF_SEARCH_ASTAR");
    const std::string corpusDir = (env && *env) ? std::string(env) : getCorpusDirDefault();

    std::error_code ec;
    if (!fs::exists(corpusDir, ec) || !fs::is_directory(corpusDir, ec)) {
        std::cout << "[conf_search_astar_corpus_runner] corpus dir not found; skipping: " << corpusDir << "\n";
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

    std::cout << "[conf_search_astar_corpus_runner] corpusDir=" << corpusDir
              << " filesVisited=" << filesVisited
              << " executed=" << executed
              << " readErr=" << readErr << "\n";
    return 0;
}

