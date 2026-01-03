#pragma once

#include <cstdint>
#include <optional>
#include <vector>

namespace osprey::kstar {

// Minimal C++ analogue of Java `edu.duke.cs.osprey.confspace.ConfSearch`.
struct ScoredConf final {
    std::vector<int32_t> assignments;
    double score = 0.0;
};

class ConfSearch {
public:
    virtual ~ConfSearch() = default;

    // Returns next conformation, or std::nullopt when exhausted.
    virtual std::optional<ScoredConf> nextConf() = 0;

    // Total number of conformations in this search.
    virtual std::uint64_t getNumConformations() const = 0;
};

} // namespace osprey::kstar


