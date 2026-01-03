#pragma once

#include <cstdlib>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace osprey::kstar::testutil {

// Resolve a test-data file path in a way that works across multiple build trees.
//
// Motivation: we often generate `test_data/*.bin` into one build directory (e.g. build/cpp/kstar/test_data),
// but we also run tests from sibling build dirs (e.g. build/cpp/kstar-coverage) where that directory
// doesn't exist, causing data-backed tests to be skipped.
//
// Search order:
//  1) If env var OSPREY_KSTAR_TEST_DATA_DIR is set, treat it as the test_data root.
//  2) Search upward from current working directory for:
//     - <dir>/test_data/<relativeFile>
//     - <dir>/kstar/test_data/<relativeFile>           (sibling build)
//     - <dir>/kstar-coverage/test_data/<relativeFile>  (sibling build)
//     - <dir>/kstar-fuzz/test_data/<relativeFile>      (sibling build)
//     - <dir>/src/test_data/kstar/<relativeFile>       (repo-committed test inputs)
//
// `relativeFile` may be either:
//  - "foo.bin"
//  - "subdir/foo.bin"
//  - "test_data/foo.bin" (the leading "test_data/" will be stripped)
inline std::optional<std::filesystem::path> resolveTestDataPath(std::string_view relativeFile) {
    namespace fs = std::filesystem;

    auto stripPrefix = [](std::string_view s, std::string_view prefix) -> std::string_view {
        if (s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix) {
            return s.substr(prefix.size());
        }
        return s;
    };

    relativeFile = stripPrefix(relativeFile, "test_data/");
    relativeFile = stripPrefix(relativeFile, "./test_data/");

    std::vector<fs::path> roots;
    roots.reserve(1 + 6 * 4);

    if (const char* env = std::getenv("OSPREY_KSTAR_TEST_DATA_DIR")) {
        if (*env != '\0') {
            roots.emplace_back(env);
        }
    }

    fs::path dir = fs::current_path();
    for (int depth = 0; depth < 8; ++depth) {
        roots.emplace_back(dir / "test_data");
        roots.emplace_back(dir / "kstar" / "test_data");
        roots.emplace_back(dir / "kstar-coverage" / "test_data");
        roots.emplace_back(dir / "kstar-fuzz" / "test_data");
        roots.emplace_back(dir / "src" / "test_data" / "kstar");

        if (!dir.has_parent_path()) {
            break;
        }
        fs::path parent = dir.parent_path();
        if (parent == dir) {
            break;
        }
        dir = std::move(parent);
    }

    for (const auto& root : roots) {
        fs::path p = root / fs::path(relativeFile);
        std::error_code ec;
        if (fs::exists(p, ec) && !ec) {
            return p;
        }
    }

    return std::nullopt;
}

inline std::string describeTestDataSearch(std::string_view relativeFile) {
    namespace fs = std::filesystem;
    std::string out;
    out.reserve(512);

    out += "Tried resolving test data file: ";
    out += std::string(relativeFile);
    out += "\n";
    out += "Hint: set env var OSPREY_KSTAR_TEST_DATA_DIR to your test_data directory.\n";
    out += "Current working directory: ";
    out += fs::current_path().string();
    out += "\n";
    return out;
}

} // namespace osprey::kstar::testutil


