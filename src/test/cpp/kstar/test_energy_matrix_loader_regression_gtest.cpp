#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

#include <filesystem>
#include <fstream>
#include <vector>

#include <gtest/gtest.h>

namespace {

std::vector<std::uint8_t> readAllBytes(const std::filesystem::path& p) {
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

} // namespace

TEST(EnergyMatrixLoaderRegression, PromotedFuzzInputsDontCrash) {
    namespace fs = std::filesystem;

    // This regression test intentionally does NOT require committed binary inputs.
    //
    // It always runs a small embedded seed corpus to ensure the parser never crashes on common
    // boundary/truncation patterns. If you have additional corpus inputs (e.g., from fuzzing),
    // place them under a resolvable directory such as:
    //   build/cpp/kstar/test_data/fuzz/energy_matrix_loader/
    // and they will be replayed as well.
    //
    // Use testutil resolution so this works from multiple build dirs (coverage, fuzz, etc).
    std::optional<fs::path> dirOpt = osprey::kstar::testutil::resolveTestDataPath("fuzz/energy_matrix_loader");

    auto runOne = [](const std::uint8_t* data, std::size_t size) {
        // Regression contract: this should not crash (exceptions are allowed until a specific bug is fixed).
        try {
            (void)osprey::kstar::EnergyMatrixLoader<double>::loadFromBytes(data, size);
        } catch (const std::exception&) {
            // acceptable: invalid inputs are expected to fail fast via exceptions
        }
    };

    // Minimal embedded seeds (avoid committing binaries, still protects against regressions).
    // These are not intended to be exhaustive; libFuzzer grows the real corpus.
    const std::vector<std::vector<std::uint8_t>> embeddedSeeds = {
        {},                         // empty
        {0x00},                     // 1 byte
        {0x00, 0x00, 0x00},         // truncated header
        std::vector<std::uint8_t>(8, 0x00),   // constTerm present, dims missing
        std::vector<std::uint8_t>(12, 0x00),  // constTerm + partial numPositions
        std::vector<std::uint8_t>(16, 0x00),  // constTerm + numPositions + partial conf counts
        // A slightly larger all-zero blob: exercises bounds checks deeper without being "valid".
        std::vector<std::uint8_t>(64, 0x00),
    };

    for (const auto& seed : embeddedSeeds) {
        runOne(seed.data(), seed.size());
    }

    int numFiles = 0;
    if (dirOpt && fs::exists(*dirOpt) && fs::is_directory(*dirOpt)) {
        const fs::path dir = *dirOpt;
        for (const auto& ent : fs::directory_iterator(dir)) {
            if (!ent.is_regular_file()) {
                continue;
            }
            const fs::path p = ent.path();
            if (p.extension() != ".bin") {
                continue;
            }
            numFiles++;
            const auto bytes = readAllBytes(p);
            runOne(bytes.data(), bytes.size());
        }
    }
}


