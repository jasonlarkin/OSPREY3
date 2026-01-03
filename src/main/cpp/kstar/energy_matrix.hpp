#ifndef OSPREY_KSTAR_ENERGY_MATRIX_HPP
#define OSPREY_KSTAR_ENERGY_MATRIX_HPP

#include <vector>
#include <cstdint>
#include <concepts>
#include <stdexcept>

namespace osprey {
namespace kstar {

/**
 * Energy matrix structure for storing pre-computed pairwise energies.
 * 
 * Matches Java EnergyMatrix structure:
 * - One-body energies: E(pos, conf)
 * - Pairwise energies: E(pos1, conf1, pos2, conf2) where pos1 > pos2
 * 
 * Storage uses flat arrays with computed indices (matching Java TupleMatrixDouble).
 */
template<std::floating_point T>
class EnergyMatrix {
public:
    EnergyMatrix() = default;
    
    /**
     * Construct energy matrix with specified dimensions.
     * 
     * @param num_positions Number of design positions
     * @param num_confs_per_pos Number of conformations per position
     */
    EnergyMatrix(int32_t num_positions, const std::vector<int32_t>& num_confs_per_pos);
    
    /**
     * Get one-body energy for position and conformation.
     */
    [[nodiscard]] T getOneBody(int32_t pos, int32_t conf) const;
    
    /**
     * Set one-body energy for position and conformation.
     */
    void setOneBody(int32_t pos, int32_t conf, T energy);
    
    /**
     * Get pairwise energy between two conformations.
     * 
     * @param pos1 First position (must be > pos2)
     * @param conf1 Conformation at first position
     * @param pos2 Second position (must be < pos1)
     * @param conf2 Conformation at second position
     */
    [[nodiscard]] T getPairwise(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2) const;
    
    /**
     * Set pairwise energy between two conformations.
     * 
     * @param pos1 First position (must be > pos2)
     * @param conf1 Conformation at first position
     * @param pos2 Second position (must be < pos1)
     * @param conf2 Conformation at second position
     * @param energy Pairwise energy value
     */
    void setPairwise(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2, T energy);
    
    /**
     * Get number of positions.
     */
    [[nodiscard]] int32_t getNumPositions() const { return num_positions_; }
    
    /**
     * Get number of conformations at a position.
     */
    [[nodiscard]] int32_t getNumConfsAtPos(int32_t pos) const;
    
    /**
     * Get constant term (offset added to all energies).
     */
    [[nodiscard]] T getConstTerm() const { return const_term_; }
    
    /**
     * Set constant term.
     */
    void setConstTerm(T val) { const_term_ = val; }
    
    /**
     * Compute total energy for a conformation (array of conformation indices per position).
     */
    [[nodiscard]] T computeEnergy(const std::vector<int32_t>& conf) const;
    
private:
    int32_t num_positions_;
    std::vector<int32_t> num_confs_per_pos_;
    std::vector<T> one_body_;
    std::vector<T> pairwise_;
    T const_term_ = T(0);
    
    /**
     * Compute one-body index: pos * max_confs + conf
     * Actually uses cumulative offsets for variable conf counts per position.
     */
    [[nodiscard]] int32_t getOneBodyIndex(int32_t pos, int32_t conf) const;
    
    /**
     * Compute pairwise index.
     * Java uses: res1*(res1-1)/2 + res2 for position pair,
     * then conf1 * num_confs2 + conf2 for conformation pair.
     */
    [[nodiscard]] int32_t getPairwiseIndex(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2) const;
    
    /**
     * Validate position and conformation indices.
     */
    void validatePos(int32_t pos) const;
    void validateConf(int32_t pos, int32_t conf) const;
};

// Explicit instantiations
extern template class EnergyMatrix<double>;
extern template class EnergyMatrix<float>;

} // namespace kstar
} // namespace osprey

#endif // OSPREY_KSTAR_ENERGY_MATRIX_HPP

