#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <fstream>
#include <optional>
#include <string>
#include <vector>
#include <limits>
#include <stdexcept>

#include "conf_ranker.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

static std::optional<EnergyMatrix<double>> tryLoadEmat(const std::string& emat_path) {
    auto resolved = osprey::kstar::testutil::resolveTestDataPath(emat_path);
    if (!resolved) {
        return std::nullopt;
    }
    std::ifstream file(resolved->string());
    if (!file.good()) {
        return std::nullopt;
    }
    file.close();
    return EnergyMatrixLoader<double>::loadFromFile(resolved->string());
}

static int32_t swap_endian_int32(int32_t v) {
    const uint32_t x = static_cast<uint32_t>(v);
    const uint32_t y = ((x & 0x000000FFu) << 24) |
                       ((x & 0x0000FF00u) << 8) |
                       ((x & 0x00FF0000u) >> 8) |
                       ((x & 0xFF000000u) >> 24);
    return static_cast<int32_t>(y);
}

static int32_t readBeInt32(std::ifstream& in) {
    int32_t v = 0;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!in.good()) {
        throw std::runtime_error("Failed to read int32");
    }
    // Java DataOutputStream writes big-endian
    if constexpr (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__) {
        return swap_endian_int32(v);
    } else {
        return v;
    }
}

static std::optional<std::vector<std::vector<int32_t>>> tryLoadRcs(const std::string& rcs_path) {
    auto resolved = osprey::kstar::testutil::resolveTestDataPath(rcs_path);
    if (!resolved) {
        return std::nullopt;
    }

    std::ifstream in(resolved->string(), std::ios::binary);
    if (!in.good()) {
        return std::nullopt;
    }

    const int32_t numPos = readBeInt32(in);
    if (numPos < 0) {
        throw std::runtime_error("Invalid numPos in RCs file");
    }

    std::vector<std::vector<int32_t>> rcsAtPos;
    rcsAtPos.resize(static_cast<size_t>(numPos));
    for (int32_t pos = 0; pos < numPos; pos++) {
        const int32_t len = readBeInt32(in);
        if (len < 0) {
            throw std::runtime_error("Invalid rc list length in RCs file");
        }
        auto& vec = rcsAtPos[static_cast<size_t>(pos)];
        vec.resize(static_cast<size_t>(len));
        for (int32_t i = 0; i < len; i++) {
            vec[static_cast<size_t>(i)] = readBeInt32(in);
        }
    }
    return rcsAtPos;
}

static ConfRanker::BigInt u64FromDec(const std::string& s) {
    ConfRanker::BigInt v = 0;
    for (char c : s) {
        if (c < '0' || c > '9') {
            continue;
        }
        const ConfRanker::BigInt digit = static_cast<ConfRanker::BigInt>(c - '0');
        if (v > (std::numeric_limits<ConfRanker::BigInt>::max() - digit) / 10) {
            throw std::overflow_error("u64FromDec overflow (need big integer)");
        }
        v = v * 10 + digit;
    }
    return v;
}

static std::vector<std::vector<int32_t>> enumerateAllConfs(const std::vector<std::vector<int32_t>>& rcsAtPos) {
    std::vector<std::vector<int32_t>> confs;
    std::vector<int32_t> cur(static_cast<size_t>(rcsAtPos.size()), 0);

    std::function<void(size_t)> rec = [&](size_t i) {
        if (i == rcsAtPos.size()) {
            confs.push_back(cur);
            return;
        }
        for (int32_t rc : rcsAtPos[i]) {
            cur[i] = rc;
            rec(i + 1);
        }
    };
    rec(0);
    return confs;
}

static ConfRanker::BigInt bruteForceCountAtMost(
    const EnergyMatrix<double>& emat,
    const std::vector<std::vector<int32_t>>& rcsAtPos,
    const double queryScore
) {
    std::vector<int32_t> conf(static_cast<size_t>(rcsAtPos.size()), 0);
    ConfRanker::BigInt count = 0;

    std::function<void(size_t)> rec = [&](size_t posi) {
        if (posi == rcsAtPos.size()) {
            const double e = emat.computeEnergy(conf);
            if (e <= queryScore) {
                count++;
            }
            return;
        }
        for (int32_t rc : rcsAtPos[posi]) {
            conf[posi] = rc;
            rec(posi + 1);
        }
    };

    rec(0);
    return count;
}

static void checkEveryConf_VERBATIM(
    const EnergyMatrix<double>& emat,
    const std::vector<std::vector<int32_t>>& rcsAtPos,
    const ConfRanker::BigInt& total_expected // informational only
) {
    (void)total_expected;

    // Exhaustively enumerate all conformations, sort by exact energy.
    auto confs = enumerateAllConfs(rcsAtPos);
    std::vector<std::pair<double, std::vector<int32_t>>> scored;
    scored.reserve(confs.size());
    for (auto& conf : confs) {
        scored.emplace_back(emat.computeEnergy(conf), conf);
    }
    std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    ConfRanker ranker(emat, rcsAtPos);

    int32_t expectedNumConfs = 0;
    for (const auto& [score, conf] : scored) {
        (void)conf;
        expectedNumConfs++;

        // roundoff error makes using the exact conf energies slightly unstable, so allow off-by-one errors
        const auto observed_u64 = ranker.getNumConfsAtMost(score);
        ASSERT_LE(observed_u64, static_cast<ConfRanker::BigInt>(std::numeric_limits<int32_t>::max()));
        const int32_t observed = static_cast<int32_t>(observed_u64);
        EXPECT_LE(expectedNumConfs - observed, 1) << "score=" << score;

        // using a slight epsilon on both sides should be perfectly accurate though
        const double epsilon = 0.00001;
        const auto below_u64 = ranker.getNumConfsAtMost(score - epsilon);
        const auto at_u64 = ranker.getNumConfsAtMost(score + epsilon);
        ASSERT_LE(below_u64, static_cast<ConfRanker::BigInt>(std::numeric_limits<int32_t>::max()));
        ASSERT_LE(at_u64, static_cast<ConfRanker::BigInt>(std::numeric_limits<int32_t>::max()));
        const int32_t below = static_cast<int32_t>(below_u64);
        const int32_t at = static_cast<int32_t>(at_u64);
        EXPECT_EQ(below, expectedNumConfs - 1) << "score=" << score;
        EXPECT_EQ(at, expectedNumConfs) << "score=" << score;
    }
}

// VERBATIM PORT of:
// src/test/java/edu/duke/cs/osprey/astar/TestConfRanker.java
//
// Notes:
// - Java computes the EnergyMatrix on the fly from a SimpleConfSpace created from /1CC8.ss.pdb.
// - C++ loads a Java-exported EnergyMatrix binary for the exact same ConfSpace definition.
// - For tiny/small we do an exhaustive sorted enumeration in C++ (equivalent to Java's ConfAStarTree iteration).

TEST(ConfRanker_VERBATIM, TinyDiscrete1CC8) {
    const std::string emat_path = "test_data/1CC8.TestConfRanker.tinyDiscrete.emat.bin";
    const std::string rcs_path = "test_data/1CC8.TestConfRanker.tinyDiscrete.rcs.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    auto rcsOpt = tryLoadRcs(rcs_path);
    if (!ematOpt || !rcsOpt) {
        GTEST_SKIP() << "Test data not found:\n  emat: " << emat_path << "\n  rcs: " << rcs_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestConfRankerTinyDiscrete1CC8() to generate";
    }
    auto emat = *std::move(ematOpt);
    auto rcsAtPos = *std::move(rcsOpt);

    // 8 confs
    checkEveryConf_VERBATIM(emat, rcsAtPos, u64FromDec("8"));
}

TEST(ConfRanker_VERBATIM, Small1CC8) {
    const std::string emat_path = "test_data/1CC8.TestConfRanker.small.emat.bin";
    const std::string rcs_path = "test_data/1CC8.TestConfRanker.small.rcs.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    auto rcsOpt = tryLoadRcs(rcs_path);
    if (!ematOpt || !rcsOpt) {
        GTEST_SKIP() << "Test data not found:\n  emat: " << emat_path << "\n  rcs: " << rcs_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestConfRankerSmall1CC8() to generate";
    }
    auto emat = *std::move(ematOpt);
    auto rcsAtPos = *std::move(rcsOpt);

    // 2268 confs
    checkEveryConf_VERBATIM(emat, rcsAtPos, u64FromDec("2268"));
}

TEST(ConfRanker_VERBATIM, Medium1CC8_ZeroRank) {
    const std::string emat_path = "test_data/1CC8.TestConfRanker.medium.emat.bin";
    const std::string rcs_path = "test_data/1CC8.TestConfRanker.medium.rcs.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    auto rcsOpt = tryLoadRcs(rcs_path);
    if (!ematOpt || !rcsOpt) {
        GTEST_SKIP() << "Test data not found:\n  emat: " << emat_path << "\n  rcs: " << rcs_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestConfRankerMedium1CC8() to generate";
    }
    auto emat = *std::move(ematOpt);
    auto rcsAtPos = *std::move(rcsOpt);

    ConfRanker ranker(emat, rcsAtPos);
    EXPECT_EQ(ranker.getNumConfsAtMost(0.0), u64FromDec("40306"));
}

TEST(ConfRanker_VERBATIM, Large1CC8_ZeroRank) {
    const std::string emat_path = "test_data/1CC8.TestConfRanker.large.emat.bin";
    const std::string rcs_path = "test_data/1CC8.TestConfRanker.large.rcs.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    auto rcsOpt = tryLoadRcs(rcs_path);
    if (!ematOpt || !rcsOpt) {
        GTEST_SKIP() << "Test data not found:\n  emat: " << emat_path << "\n  rcs: " << rcs_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestConfRankerLarge1CC8() to generate";
    }
    auto emat = *std::move(ematOpt);
    auto rcsAtPos = *std::move(rcsOpt);

    ConfRanker ranker(emat, rcsAtPos);
    // VERBATIM from Java `TestConfRanker.large1CC8()` in this repo (see notes there).
    const auto expected = u64FromDec("1034628");
    const auto observed = ranker.getNumConfsAtMost(0.0);
    if (observed != expected) {
        // Diagnostic: if this fails by 1, brute-force the rank to separate
        // "ranker algorithm mismatch" from "exported emat mismatch".
        const auto brute = bruteForceCountAtMost(emat, rcsAtPos, 0.0);
        ADD_FAILURE() << "ConfRanker count mismatch at queryScore=0.0\n"
                      << "  observed(rank): " << observed << "\n"
                      << "  expected(rank): " << expected << "\n"
                      << "  brute_force:    " << brute;
        ASSERT_EQ(brute, expected) << "Exported EnergyMatrix/RCs do not reproduce Java expected rank.";
    }
    EXPECT_EQ(observed, expected);
}

TEST(ConfRanker_VERBATIM, Huge1CC8_ZeroRank) {
    const std::string emat_path = "test_data/1CC8.TestConfRanker.huge.emat.bin";
    const std::string rcs_path = "test_data/1CC8.TestConfRanker.huge.rcs.bin";
    auto ematOpt = tryLoadEmat(emat_path);
    auto rcsOpt = tryLoadRcs(rcs_path);
    if (!ematOpt || !rcsOpt) {
        GTEST_SKIP() << "Test data not found:\n  emat: " << emat_path << "\n  rcs: " << rcs_path
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestConfRankerHuge1CC8() to generate";
    }
    auto emat = *std::move(ematOpt);
    auto rcsAtPos = *std::move(rcsOpt);

    ConfRanker ranker(emat, rcsAtPos);
    EXPECT_EQ(ranker.getNumConfsAtMost(0.0), u64FromDec("21039231"));
}


