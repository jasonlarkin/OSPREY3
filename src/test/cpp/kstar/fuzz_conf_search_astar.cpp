// Fuzz target: ConfSearch backed by A* (baseline or fast) on tiny synthetic EnergyMatrix instances.
//
// Goal: exercise branchy graph/tree search logic (expand ordering, pruning, edge conditions)
// without requiring Java-exported binary formats.

#include "conf_search_astar.hpp"
#include "astar_search_fast.hpp"
#include "astar_node_fast.hpp"
#include "energy_matrix.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
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

static void probe_astar_search_fast_ranges(
    const osprey::kstar::AStarSearchFast<double>& astar_fast,
    const std::vector<int32_t>& num_confs_per_pos,
    Cursor& c
) {
    const int32_t num_positions = static_cast<int32_t>(num_confs_per_pos.size());
    if (num_positions <= 1) return;

    // Previously this harness directly called sumUndefinedRange(...), but that's intentionally private.
    // Instead, drive the same heuristic code paths via the public API: computeHScore/expandInto.
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
        __builtin_trap();
    }
}

static void run_one(const std::uint8_t* data, std::size_t size) {
    if (data == nullptr || size < 8) {
        return;
    }

    Cursor c{data, size, 0};

    const int32_t num_positions = 2 + static_cast<int32_t>(c.u8() % 9); // 2..10

    // Config bits:
    // - bit0: prefer fast variant for the primary run
    // - bit1: cross-check (run both baseline+fast and compare first-N score sets)
    // - bit2: extreme energies (larger magnitudes)
    // - bit3: tie-heavy energies (lots of equal values)
    const std::uint8_t cfg = c.u8();
    const bool prefer_fast = (cfg & 0x01) != 0;
    const bool cross_check = (cfg & 0x02) != 0;
    const bool extreme = (cfg & 0x04) != 0;
    const bool tie_heavy = (cfg & 0x08) != 0;

    std::vector<int32_t> num_confs_per_pos(static_cast<std::size_t>(num_positions), 1);
    std::uint64_t total_confs = 1;
    std::uint64_t total_pairwise_terms = 0;
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        const int32_t nconf = 1 + static_cast<int32_t>(c.u8() % 8); // 1..8
        num_confs_per_pos[static_cast<std::size_t>(pos)] = nconf;
        total_confs *= static_cast<std::uint64_t>(nconf);
        if (total_confs > 20000) {
            // Keep the search bounded for fuzzing (and avoid huge allocations).
            return;
        }
    }
    for (int32_t pos1 = 1; pos1 < num_positions; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            total_pairwise_terms += static_cast<std::uint64_t>(num_confs_per_pos[static_cast<std::size_t>(pos1)]) *
                                   static_cast<std::uint64_t>(num_confs_per_pos[static_cast<std::size_t>(pos2)]);
            if (total_pairwise_terms > 120000) {
                // Bound time/memory spent on pairwise matrix fill.
                return;
            }
        }
    }

    // When cross-checking, keep the enumeration relatively small to avoid O(N log N) overhead in the harness.
    if (cross_check && total_confs > 4096) {
        return;
    }

    osprey::kstar::EnergyMatrix<double> emat(num_positions, num_confs_per_pos);

    const double denom_const = extreme ? 1.0 : 16.0;
    const double denom_one = extreme ? 1.0 : 8.0;
    const double denom_pair = extreme ? 1.0 : 16.0;

    // Const term.
    emat.setConstTerm(scale_i8(c.i8(), denom_const));

    // One-body terms.
    for (int32_t pos = 0; pos < num_positions; ++pos) {
        const int32_t nconf = num_confs_per_pos[static_cast<std::size_t>(pos)];
        for (int32_t rc = 0; rc < nconf; ++rc) {
            const double v = tie_heavy ? 0.0 : scale_i8(c.i8(), denom_one);
            emat.setOneBody(pos, rc, v);
        }
    }

    // Pairwise terms (pos1 > pos2).
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

            // Invariant: assignments are in range.
            if (static_cast<int32_t>(next->assignments.size()) != num_positions) {
                __builtin_trap();
            }
            for (int32_t pos = 0; pos < num_positions; ++pos) {
                const int32_t rc = next->assignments[static_cast<std::size_t>(pos)];
                const int32_t nconf = num_confs_per_pos[static_cast<std::size_t>(pos)];
                if (rc < 0 || rc >= nconf) {
                    __builtin_trap();
                }
            }

            // Invariant: non-decreasing A* score sequence (allow tiny numeric noise).
            if (next->score + 1e-12 < prev_score) {
                __builtin_trap();
            }
            prev_score = next->score;

            Out out;
            out.score = next->score;
            out.assignments = next->assignments;
            outs.emplace_back(std::move(out));
        }
        return outs;
    };

    // Force coverage through AStarSearchFast boundary logic even if this input prefers baseline.
    // This helps fuzz-corpus replay provide new prod coverage deltas that unit tests may not hit.
    {
        auto astar_fast = osprey::kstar::AStarSearchFast<double>(
            emat, num_positions, std::span<const int32_t>(num_confs_per_pos.data(), num_confs_per_pos.size())
        );
        probe_astar_search_fast_ranges(astar_fast, num_confs_per_pos, c);
    }

    const auto primary_variant = prefer_fast ? osprey::kstar::AStarVariant::Fast : osprey::kstar::AStarVariant::Baseline;
    const std::uint64_t max_steps = (total_confs < 2048) ? total_confs : 2048;

    // Always run once (this is what gives coverage).
    (void)run_collect_variant(primary_variant, max_steps);

    // Determinism check: for a fixed input and variant, the search should be repeatable.
    const std::uint64_t det_steps = (max_steps < 16) ? max_steps : 16;
    const auto da = run_collect_variant(primary_variant, det_steps);
    const auto db = run_collect_variant(primary_variant, det_steps);
    if (da.size() != db.size()) __builtin_trap();
    for (std::size_t i = 0; i < da.size(); ++i) {
        if (da[i].assignments != db[i].assignments) __builtin_trap();
        if (std::fabs(da[i].score - db[i].score) > 1e-12) __builtin_trap();
    }

    if (cross_check) {
        // Cross-check baseline vs fast agree on the multiset of first-N scores (tie-order agnostic).
        // This tends to stress deeper expansion/pruning logic and catches subtle divergences.
        const std::uint64_t n = (max_steps < 32) ? max_steps : 32;
        auto a = run_collect_variant(osprey::kstar::AStarVariant::Baseline, n);
        auto b = run_collect_variant(osprey::kstar::AStarVariant::Fast, n);

        std::vector<double> sa;
        std::vector<double> sb;
        sa.reserve(a.size());
        sb.reserve(b.size());
        for (const auto& o : a) sa.push_back(o.score);
        for (const auto& o : b) sb.push_back(o.score);
        std::sort(sa.begin(), sa.end());
        std::sort(sb.begin(), sb.end());

        if (sa.size() != sb.size()) __builtin_trap();
        for (std::size_t i = 0; i < sa.size(); ++i) {
            if (std::fabs(sa[i] - sb[i]) > 1e-9) {
                __builtin_trap();
            }
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

