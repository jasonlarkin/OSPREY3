#include "energy_matrix_loader.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace fs = std::filesystem;

static std::string getCorpusDirDefault() {
    // Default to the standard fuzz build location under the repo root.
    // This keeps the "fuzz -> coverage" workflow zero-config in CI and locally.
    return std::string(OSPREY_REPO_ROOT) + "/build/cpp/kstar-fuzz/fuzz-corpus/energy_matrix_loader";
}

int main() {
    const char* env = std::getenv("KSTAR_FUZZ_CORPUS_DIR");
    const std::string corpusDir = (env && *env) ? std::string(env) : getCorpusDirDefault();

    const bool trace = (std::getenv("KSTAR_CORPUS_RUNNER_TRACE") != nullptr);
    std::optional<std::size_t> limit;
    if (const char* lim = std::getenv("KSTAR_CORPUS_RUNNER_LIMIT")) {
        try {
            limit = static_cast<std::size_t>(std::stoull(lim));
        } catch (...) {
            // ignore invalid
        }
    }

    std::error_code ec;
    if (!fs::exists(corpusDir, ec) || !fs::is_directory(corpusDir, ec)) {
        std::cout << "[energy_matrix_loader_corpus_runner] corpus dir not found; skipping: " << corpusDir << "\n";
        return 0;
    }

    std::size_t filesVisited = 0;
    std::size_t parsedOk = 0;
    std::size_t parsedErr = 0;

    for (const auto& entry : fs::directory_iterator(corpusDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (limit && filesVisited >= *limit) break;
        ++filesVisited;

        const auto path = entry.path();
        if (trace) {
            std::cout << "[energy_matrix_loader_corpus_runner] file=" << path.string() << "\n" << std::flush;
        }
        // Skip extremely large inputs to keep coverage runs bounded.
        const auto size = entry.file_size(ec);
        if (!ec && size > 20 * 1024 * 1024) {
            continue;
        }

        try {
            (void)osprey::kstar::EnergyMatrixLoader<double>::loadFromFile(path.string());
            ++parsedOk;
        } catch (...) {
            ++parsedErr;
        }
    }

    std::cout << "[energy_matrix_loader_corpus_runner] corpusDir=" << corpusDir
              << " filesVisited=" << filesVisited
              << " parsedOk=" << parsedOk
              << " parsedErr=" << parsedErr << "\n";
    return 0;
}


