#include <gtest/gtest.h>

#include <cstdint>
#include <fstream>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>

#include "conf_search.hpp"
#include "conf_search_cache.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

static int32_t swap_endian_int32(int32_t v) {
    const uint32_t x = static_cast<uint32_t>(v);
    const uint32_t y = ((x & 0x000000FFu) << 24) |
                       ((x & 0x0000FF00u) << 8) |
                       ((x & 0x00FF0000u) >> 8) |
                       ((x & 0xFF000000u) >> 24);
    return static_cast<int32_t>(y);
}

static uint64_t swap_endian_u64(uint64_t v) {
    uint64_t x = v;
    x = ((x & 0x00000000000000FFull) << 56) |
        ((x & 0x000000000000FF00ull) << 40) |
        ((x & 0x0000000000FF0000ull) << 24) |
        ((x & 0x00000000FF000000ull) << 8) |
        ((x & 0x000000FF00000000ull) >> 8) |
        ((x & 0x0000FF0000000000ull) >> 24) |
        ((x & 0x00FF000000000000ull) >> 40) |
        ((x & 0xFF00000000000000ull) >> 56);
    return x;
}

static double swap_endian_double(double v) {
    static_assert(sizeof(double) == sizeof(uint64_t));
    uint64_t x = 0;
    std::memcpy(&x, &v, sizeof(x));
    x = swap_endian_u64(x);
    std::memcpy(&v, &x, sizeof(x));
    return v;
}

static int32_t readBeInt32(std::ifstream& in) {
    int32_t v = 0;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!in.good()) {
        throw std::runtime_error("Failed to read int32");
    }
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return swap_endian_int32(v);
#else
    return v;
#endif
}

static double readBeDouble(std::ifstream& in) {
    double v = 0;
    in.read(reinterpret_cast<char*>(&v), sizeof(v));
    if (!in.good()) {
        throw std::runtime_error("Failed to read double");
    }
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    return swap_endian_double(v);
#else
    return v;
#endif
}

static std::optional<std::vector<ScoredConf>> tryLoadExpectedConfs(const std::string& path) {
    auto resolved = osprey::kstar::testutil::resolveTestDataPath(path);
    if (!resolved) {
        return std::nullopt;
    }
    std::ifstream in(resolved->string(), std::ios::binary);
    if (!in.good()) {
        return std::nullopt;
    }

    const int32_t n = readBeInt32(in);
    if (n < 0) {
        throw std::runtime_error("Invalid conf count in expected confs file");
    }

    std::vector<ScoredConf> out;
    out.reserve(static_cast<size_t>(n));

    for (int32_t i = 0; i < n; i++) {
        const int32_t len = readBeInt32(in);
        if (len < 0) {
            throw std::runtime_error("Invalid assignments length in expected confs file");
        }
        ScoredConf conf;
        conf.assignments.resize(static_cast<size_t>(len));
        for (int32_t j = 0; j < len; j++) {
            conf.assignments[static_cast<size_t>(j)] = readBeInt32(in);
        }
        conf.score = readBeDouble(in);
        out.push_back(std::move(conf));
    }

    return out;
}

class VectorConfSearch final : public ConfSearch {
public:
    explicit VectorConfSearch(std::vector<ScoredConf> confs)
        : confs_(std::move(confs)) {}

    std::optional<ScoredConf> nextConf() override {
        if (idx_ >= confs_.size()) {
            return std::nullopt;
        }
        return confs_[idx_++]; // copy out to match Java semantics
    }

    std::uint64_t getNumConformations() const override {
        return static_cast<std::uint64_t>(confs_.size());
    }

private:
    std::vector<ScoredConf> confs_;
    size_t idx_ = 0;
};

static void expectConfEq(const ScoredConf& a, const ScoredConf& b) {
    EXPECT_EQ(a.assignments, b.assignments);
    EXPECT_DOUBLE_EQ(a.score, b.score);
}

// VERBATIM PORT of:
// src/test/java/edu/duke/cs/osprey/astar/TestConfSearchCache.java
//
// Note: Java test computes expected confs from ConfAStarTree. In C++ we load the expected conf list
// exported from Java for verbatim comparison, then validate ConfSearchCache semantics.

TEST(ConfSearchCache_VERBATIM, TreeReinstantiation) {
    const std::string expected_path = "test_data/1CC8.TestConfSearchCache.expected_confs.bin";
    auto expectedOpt = tryLoadExpectedConfs(expected_path);
    if (!expectedOpt) {
        GTEST_SKIP() << "Expected confs file not found: " << expected_path << "\n"
                     << osprey::kstar::testutil::describeTestDataSearch(expected_path)
                     << "\nRun Java test ExportEnergyMatrixForCppTest.exportTestConfSearchCacheExpectedConfs() to generate";
    }
    auto expected = *std::move(expectedOpt);
    ASSERT_EQ(expected.size(), 27u);

    ConfSearchCache cache(1);
    auto entry = cache.make([expected]() mutable {
        return std::make_unique<VectorConfSearch>(expected);
    });

    EXPECT_EQ(entry.getNumConformations(), expected.size());

    for (int i = 0; i < 10; i++) {
        auto conf = entry.nextConf();
        ASSERT_TRUE(conf.has_value());
        expectConfEq(conf.value(), expected[static_cast<size_t>(i)]);
    }

    // clear the trees and force re-instantiation
    entry.clearRefs();

    for (int i = 10; i < 20; i++) {
        auto conf = entry.nextConf();
        ASSERT_TRUE(conf.has_value());
        expectConfEq(conf.value(), expected[static_cast<size_t>(i)]);
    }

    // clear the trees and force re-instantiation again, just for fun
    entry.clearRefs();

    for (int i = 20; i < 27; i++) {
        auto conf = entry.nextConf();
        ASSERT_TRUE(conf.has_value());
        expectConfEq(conf.value(), expected[static_cast<size_t>(i)]);
    }

    // make anything after that is all nulls
    for (int i = 27; i < 30; i++) {
        EXPECT_FALSE(entry.nextConf().has_value());
    }
}

TEST(ConfSearchCache_VERBATIM, UnrestrictedCapacity) {
    // Use trivial factories; we only test protection behavior, not enumeration.
    ConfSearchCache cache(std::nullopt);

    auto tree1 = cache.make([] { return std::make_unique<VectorConfSearch>(std::vector<ScoredConf>{}); });
    EXPECT_TRUE(tree1.isProtected());

    auto tree2 = cache.make([] { return std::make_unique<VectorConfSearch>(std::vector<ScoredConf>{}); });
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());

    auto tree3 = cache.make([] { return std::make_unique<VectorConfSearch>(std::vector<ScoredConf>{}); });
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());
    EXPECT_TRUE(tree3.isProtected());
}

TEST(ConfSearchCache_VERBATIM, RestrictedCapacity) {
    ConfSearchCache cache(static_cast<size_t>(2));

    // Important: these cached searches must not exhaust during this test, otherwise
    // `ConfSearchCache::Entry::nextConf()` is allowed to clear refs (matching Java semantics),
    // which would break the "protected" assertions.
    auto makeMany = [] {
        std::vector<ScoredConf> confs;
        confs.reserve(100);
        for (int i = 0; i < 100; i++) {
            confs.push_back(ScoredConf{{}, 0.0});
        }
        return std::make_unique<VectorConfSearch>(std::move(confs));
    };

    auto tree1 = cache.make(makeMany);
    EXPECT_TRUE(tree1.isProtected());

    auto tree2 = cache.make(makeMany);
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());

    auto tree3 = cache.make(makeMany);
    EXPECT_FALSE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());
    EXPECT_TRUE(tree3.isProtected());

    (void)tree1.nextConf();
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_FALSE(tree2.isProtected());
    EXPECT_TRUE(tree3.isProtected());

    (void)tree2.nextConf();
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());
    EXPECT_FALSE(tree3.isProtected());

    (void)tree1.nextConf();
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());
    EXPECT_FALSE(tree3.isProtected());

    (void)tree2.nextConf();
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());
    EXPECT_FALSE(tree3.isProtected());

    (void)tree1.nextConf();
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_TRUE(tree2.isProtected());
    EXPECT_FALSE(tree3.isProtected());

    (void)tree3.nextConf();
    EXPECT_TRUE(tree1.isProtected());
    EXPECT_FALSE(tree2.isProtected());
    EXPECT_TRUE(tree3.isProtected());
}


