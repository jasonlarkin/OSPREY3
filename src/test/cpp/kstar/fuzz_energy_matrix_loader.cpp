#include "energy_matrix_loader.hpp"

#include <cstddef>
#include <cstdint>

// libFuzzer entrypoint.
extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
    try {
        // The Java-exported format is double-based. Fuzz the double loader.
        // Expected failures (truncation/invalid sizes) should throw; we swallow them.
        (void)osprey::kstar::EnergyMatrixLoader<double>::loadFromBytes(data, size);
    } catch (...) {
        // ignore
    }
    return 0;
}


