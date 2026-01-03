#include "energy_matrix_loader.hpp"
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <cstdint>
#include <arpa/inet.h>  // For ntohl (network to host byte order)
#include <limits>
#include <vector>

namespace osprey {
namespace kstar {

// Java DataOutputStream writes in big-endian (network byte order)
// Convert from big-endian to native byte order
inline int32_t readBigEndianInt32(const char* bytes) {
    int32_t val;
    std::memcpy(&val, bytes, sizeof(int32_t));
    return ntohl(val);
}

inline double readBigEndianDouble(const char* bytes) {
    // Java doubles are big-endian, need to swap bytes
    // Manual byte swap for portability (ntohll not always available)
    uint64_t bits = 0;
    for (int i = 0; i < 8; ++i) {
        bits |= (static_cast<uint64_t>(static_cast<unsigned char>(bytes[i])) << (56 - 8 * i));
    }
    double val;
    std::memcpy(&val, &bits, sizeof(double));
    return val;
}

template<std::floating_point T>
EnergyMatrix<T> EnergyMatrixLoader<T>::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + filepath);
    }

    // Read entire file into memory; parsing is done by loadFromBytes so fuzzing/tests can share logic.
    file.seekg(0, std::ios::end);
    const std::streamoff endPos = file.tellg();
    if (endPos < 0) {
        throw std::runtime_error("Failed to stat EnergyMatrix file: " + filepath);
    }
    file.seekg(0, std::ios::beg);

    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(endPos));
    if (!bytes.empty()) {
        file.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        if (static_cast<std::size_t>(file.gcount()) != bytes.size()) {
            throw std::runtime_error("Failed to read EnergyMatrix bytes from: " + filepath);
        }
    }

    if (file.fail() && !file.eof()) {
        throw std::runtime_error("Error reading EnergyMatrix file: " + filepath);
    }

    return loadFromBytes(bytes.data(), bytes.size());
}

template<std::floating_point T>
EnergyMatrix<T> EnergyMatrixLoader<T>::loadFromBytes(const std::uint8_t* data, std::size_t size) {
    // The Java-exported format is always big-endian and stores floating values as Java doubles (8 bytes).
    // We parse doubles and cast to T so the public API can stay templated.
    constexpr std::size_t kI32 = 4;
    constexpr std::size_t kF64 = 8;

    // Keep fuzzing and corrupted-file handling safe: cap allocations/work.
    // These limits are intentionally conservative; adjust as needed for real datasets.
    constexpr int32_t kMaxPositions = 512;
    constexpr int32_t kMaxConfsPerPos = 1'000'000; // absolute cap to prevent overflow/oom
    constexpr std::int64_t kMaxTotalOneBody = 20'000'000;
    constexpr std::int64_t kMaxTotalPairwise = 200'000'000;

    if (data == nullptr && size != 0) {
        throw std::runtime_error("EnergyMatrixLoader::loadFromBytes: null data with non-zero size");
    }

    std::size_t off = 0;
    auto require = [&](std::size_t n) {
        if (n > size - off) {
            throw std::runtime_error("EnergyMatrixLoader::loadFromBytes: truncated input");
        }
    };
    auto readI32 = [&]() -> int32_t {
        require(kI32);
        int32_t v = readBigEndianInt32(reinterpret_cast<const char*>(data + off));
        off += kI32;
        return v;
    };
    auto readF64 = [&]() -> double {
        require(kF64);
        double v = readBigEndianDouble(reinterpret_cast<const char*>(data + off));
        off += kF64;
        return v;
    };

    const T constTerm = static_cast<T>(readF64());
    const int32_t numPositions = readI32();
    if (numPositions < 0 || numPositions > kMaxPositions) {
        throw std::runtime_error("EnergyMatrixLoader::loadFromBytes: invalid numPositions");
    }

    std::vector<int32_t> numConfsPerPos(static_cast<std::size_t>(numPositions));
    for (int32_t i = 0; i < numPositions; ++i) {
        const int32_t n = readI32();
        if (n < 0 || n > kMaxConfsPerPos) {
            throw std::runtime_error("EnergyMatrixLoader::loadFromBytes: invalid numConfsPerPos");
        }
        numConfsPerPos[static_cast<std::size_t>(i)] = n;
    }

    std::int64_t totalOneBody = 0;
    for (int32_t pos = 0; pos < numPositions; ++pos) {
        totalOneBody += static_cast<std::int64_t>(numConfsPerPos[static_cast<std::size_t>(pos)]);
        if (totalOneBody < 0 || totalOneBody > kMaxTotalOneBody) {
            throw std::runtime_error("EnergyMatrixLoader::loadFromBytes: totalOneBody too large");
        }
    }

    std::int64_t totalPairwise = 0;
    for (int32_t pos1 = 1; pos1 < numPositions; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            totalPairwise += static_cast<std::int64_t>(numConfsPerPos[static_cast<std::size_t>(pos1)]) *
                             static_cast<std::int64_t>(numConfsPerPos[static_cast<std::size_t>(pos2)]);
            if (totalPairwise < 0 || totalPairwise > kMaxTotalPairwise) {
                throw std::runtime_error("EnergyMatrixLoader::loadFromBytes: totalPairwise too large");
            }
        }
    }

    // Read oneBody doubles
    std::vector<T> oneBody(static_cast<std::size_t>(totalOneBody));
    for (std::int64_t i = 0; i < totalOneBody; ++i) {
        oneBody[static_cast<std::size_t>(i)] = static_cast<T>(readF64());
    }

    // Read pairwise doubles
    std::vector<T> pairwise(static_cast<std::size_t>(totalPairwise));
    for (std::int64_t i = 0; i < totalPairwise; ++i) {
        pairwise[static_cast<std::size_t>(i)] = static_cast<T>(readF64());
    }

    // Create EnergyMatrix and populate it
    EnergyMatrix<T> emat(numPositions, numConfsPerPos);
    emat.setConstTerm(constTerm);

    // Set one-body energies
    int32_t oneBodyIdx = 0;
    for (int32_t pos = 0; pos < numPositions; ++pos) {
        for (int32_t conf = 0; conf < numConfsPerPos[static_cast<std::size_t>(pos)]; ++conf) {
            emat.setOneBody(pos, conf, oneBody[static_cast<std::size_t>(oneBodyIdx++)]);
        }
    }

    // Set pairwise energies
    int32_t pairwiseIdx = 0;
    for (int32_t pos1 = 1; pos1 < numPositions; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            for (int32_t conf1 = 0; conf1 < numConfsPerPos[static_cast<std::size_t>(pos1)]; ++conf1) {
                for (int32_t conf2 = 0; conf2 < numConfsPerPos[static_cast<std::size_t>(pos2)]; ++conf2) {
                    emat.setPairwise(pos1, conf1, pos2, conf2, pairwise[static_cast<std::size_t>(pairwiseIdx++)]);
                }
            }
        }
    }

    return emat;
}

template<std::floating_point T>
EnergyMatrix<T> EnergyMatrixLoader<T>::loadFromData(
    T constTerm,
    int32_t numPositions,
    const std::vector<int32_t>& numConfsPerPos,
    const std::vector<T>& oneBody,
    const std::vector<T>& pairwise
) {
    EnergyMatrix<T> emat(numPositions, numConfsPerPos);
    emat.setConstTerm(constTerm);
    
    // Set one-body energies
    int32_t oneBodyIdx = 0;
    for (int32_t pos = 0; pos < numPositions; ++pos) {
        for (int32_t conf = 0; conf < numConfsPerPos[pos]; ++conf) {
            emat.setOneBody(pos, conf, oneBody[oneBodyIdx++]);
        }
    }
    
    // Set pairwise energies
    int32_t pairwiseIdx = 0;
    for (int32_t pos1 = 1; pos1 < numPositions; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            for (int32_t conf1 = 0; conf1 < numConfsPerPos[pos1]; ++conf1) {
                for (int32_t conf2 = 0; conf2 < numConfsPerPos[pos2]; ++conf2) {
                    emat.setPairwise(pos1, conf1, pos2, conf2, pairwise[pairwiseIdx++]);
                }
            }
        }
    }
    
    return emat;
}

// Explicit instantiations
template class EnergyMatrixLoader<double>;
template class EnergyMatrixLoader<float>;

} // namespace kstar
} // namespace osprey

