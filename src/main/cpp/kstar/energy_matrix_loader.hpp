#ifndef OSPREY_KSTAR_ENERGY_MATRIX_LOADER_HPP
#define OSPREY_KSTAR_ENERGY_MATRIX_LOADER_HPP

#include "energy_matrix.hpp"
#include <string>
#include <fstream>
#include <vector>
#include <concepts>
#include <cstddef>
#include <cstdint>

namespace osprey {
namespace kstar {

/**
 * Load EnergyMatrix from binary format exported by Java.
 * 
 * Format:
 * - constTerm (double)
 * - numPositions (int32_t)
 * - numConfsPerPos (int32_t[numPositions])
 * - oneBody array (double[])
 * - pairwise array (double[])
 */
template<std::floating_point T>
class EnergyMatrixLoader {
public:
    /**
     * Load EnergyMatrix from binary file exported by Java.
     */
    static EnergyMatrix<T> loadFromFile(const std::string& filepath);

    /**
     * Load EnergyMatrix from binary bytes exported by Java.
     *
     * Intended for fuzzing/tests and for callers that already have the data in memory.
     */
    static EnergyMatrix<T> loadFromBytes(const std::uint8_t* data, std::size_t size);
    
    /**
     * Load EnergyMatrix from binary data (for testing).
     */
    static EnergyMatrix<T> loadFromData(
        T constTerm,
        int32_t numPositions,
        const std::vector<int32_t>& numConfsPerPos,
        const std::vector<T>& oneBody,
        const std::vector<T>& pairwise
    );
};

// Explicit instantiations
extern template class EnergyMatrixLoader<double>;
extern template class EnergyMatrixLoader<float>;

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ENERGY_MATRIX_LOADER_HPP

