#include "energy_matrix.hpp"
#include <algorithm>
#include <numeric>

namespace osprey {
namespace kstar {

template<std::floating_point T>
EnergyMatrix<T>::EnergyMatrix(int32_t num_positions, const std::vector<int32_t>& num_confs_per_pos)
    : num_positions_(num_positions)
    , num_confs_per_pos_(num_confs_per_pos)
{
    if (num_positions_ != static_cast<int32_t>(num_confs_per_pos_.size())) {
        throw std::invalid_argument("num_positions must match num_confs_per_pos size");
    }

    // Precompute one-body offsets (prefix sums).
    one_body_offsets_.assign(static_cast<size_t>(num_positions_) + 1, 0);
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        one_body_offsets_[static_cast<size_t>(pos) + 1] =
            one_body_offsets_[static_cast<size_t>(pos)] + num_confs_per_pos_[pos];
    }
    const int32_t total_one_body = one_body_offsets_[static_cast<size_t>(num_positions_)];
    one_body_.resize(total_one_body, T(0));

    // Precompute pairwise offsets (Java-style triangular position packing).
    const int32_t num_pairs = (num_positions_ * (num_positions_ - 1)) / 2;
    pairwise_offsets_.assign(static_cast<size_t>(num_pairs), 0);

    int32_t total_pairwise = 0;
    for (int32_t pos1 = 1; pos1 < num_positions_; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            const int32_t idx = pairPosIndex(pos1, pos2);
            pairwise_offsets_[static_cast<size_t>(idx)] = total_pairwise;
            total_pairwise += num_confs_per_pos_[pos1] * num_confs_per_pos_[pos2];
        }
    }
    pairwise_.resize(total_pairwise, T(0));
}

template<std::floating_point T>
int32_t EnergyMatrix<T>::getOneBodyIndex(int32_t pos, int32_t conf) const {
    validatePos(pos);
    validateConf(pos, conf);

    return one_body_offsets_[static_cast<size_t>(pos)] + conf;
}

template<std::floating_point T>
int32_t EnergyMatrix<T>::getPairwiseIndex(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2) const {
    // Ensure pos1 > pos2 (swap if needed)
    if (pos1 < pos2) {
        std::swap(pos1, pos2);
        std::swap(conf1, conf2);
    } else if (pos1 == pos2) {
        throw std::invalid_argument("pos1 and pos2 must be different");
    }
    
    validatePos(pos1);
    validatePos(pos2);
    validateConf(pos1, conf1);
    validateConf(pos2, conf2);

    const int32_t base = pairwise_offsets_[static_cast<size_t>(pairPosIndex(pos1, pos2))];
    return base + num_confs_per_pos_[pos2] * conf1 + conf2;
}

template<std::floating_point T>
T EnergyMatrix<T>::getOneBody(int32_t pos, int32_t conf) const {
    return one_body_[getOneBodyIndex(pos, conf)];
}

template<std::floating_point T>
T EnergyMatrix<T>::getOneBodyUnchecked(int32_t pos, int32_t conf) const noexcept {
    return one_body_[one_body_offsets_[static_cast<size_t>(pos)] + conf];
}

template<std::floating_point T>
void EnergyMatrix<T>::setOneBody(int32_t pos, int32_t conf, T energy) {
    one_body_[getOneBodyIndex(pos, conf)] = energy;
}

template<std::floating_point T>
T EnergyMatrix<T>::getPairwise(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2) const {
    // Ensure pos1 > pos2
    if (pos1 < pos2) {
        std::swap(pos1, pos2);
        std::swap(conf1, conf2);
    }
    return pairwise_[getPairwiseIndex(pos1, conf1, pos2, conf2)];
}

template<std::floating_point T>
T EnergyMatrix<T>::getPairwiseAssumingPos1Greater(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2) const noexcept {
    const int32_t base = pairwise_offsets_[static_cast<size_t>(pairPosIndex(pos1, pos2))];
    return pairwise_[base + num_confs_per_pos_[pos2] * conf1 + conf2];
}

template<std::floating_point T>
std::span<const T> EnergyMatrix<T>::getPairwiseRowAssumingPos1Greater(int32_t pos1, int32_t conf1, int32_t pos2) const noexcept {
    const int32_t base = pairwise_offsets_[static_cast<size_t>(pairPosIndex(pos1, pos2))];
    const int32_t n2 = num_confs_per_pos_[pos2];
    const int32_t row0 = base + n2 * conf1;
    return std::span<const T>(&pairwise_[static_cast<size_t>(row0)], static_cast<size_t>(n2));
}

template<std::floating_point T>
typename EnergyMatrix<T>::PairwiseBlockView
EnergyMatrix<T>::getPairwiseBlockAssumingPos1Greater(int32_t pos1, int32_t pos2) const noexcept {
    const int32_t base = pairwise_offsets_[static_cast<size_t>(pairPosIndex(pos1, pos2))];
    PairwiseBlockView v;
    v.data = &pairwise_[static_cast<size_t>(base)];
    v.n1 = num_confs_per_pos_[pos1];
    v.n2 = num_confs_per_pos_[pos2];
    return v;
}

template<std::floating_point T>
T EnergyMatrix<T>::getPairwiseUnchecked(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2) const noexcept {
    if (pos1 < pos2) {
        std::swap(pos1, pos2);
        std::swap(conf1, conf2);
    }
    // pos1 != pos2 is required by contract; if equal, the index math is invalid.
    const int32_t base = pairwise_offsets_[static_cast<size_t>(pairPosIndex(pos1, pos2))];
    return pairwise_[base + num_confs_per_pos_[pos2] * conf1 + conf2];
}

template<std::floating_point T>
void EnergyMatrix<T>::setPairwise(int32_t pos1, int32_t conf1, int32_t pos2, int32_t conf2, T energy) {
    // Ensure pos1 > pos2
    if (pos1 < pos2) {
        std::swap(pos1, pos2);
        std::swap(conf1, conf2);
    }
    pairwise_[getPairwiseIndex(pos1, conf1, pos2, conf2)] = energy;
}

template<std::floating_point T>
int32_t EnergyMatrix<T>::getNumConfsAtPos(int32_t pos) const {
    validatePos(pos);
    return num_confs_per_pos_[pos];
}

template<std::floating_point T>
T EnergyMatrix<T>::computeEnergy(const std::vector<int32_t>& conf) const {
    if (static_cast<int32_t>(conf.size()) != num_positions_) {
        throw std::invalid_argument("conf size must match num_positions");
    }
    
    T energy = const_term_;
    
    // Sum one-body energies
    for (int32_t pos = 0; pos < num_positions_; ++pos) {
        energy += getOneBody(pos, conf[pos]);
    }
    
    // Sum pairwise energies (only for pos1 > pos2 to avoid double counting)
    for (int32_t pos1 = 1; pos1 < num_positions_; ++pos1) {
        for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
            energy += getPairwise(pos1, conf[pos1], pos2, conf[pos2]);
        }
    }
    
    return energy;
}

template<std::floating_point T>
void EnergyMatrix<T>::validatePos(int32_t pos) const {
    if (pos < 0 || pos >= num_positions_) {
        throw std::out_of_range("Position index out of range");
    }
}

template<std::floating_point T>
void EnergyMatrix<T>::validateConf(int32_t pos, int32_t conf) const {
    validatePos(pos);
    if (conf < 0 || conf >= num_confs_per_pos_[pos]) {
        throw std::out_of_range("Conformation index out of range");
    }
}

// Explicit instantiations
template class EnergyMatrix<double>;
template class EnergyMatrix<float>;

} // namespace kstar
} // namespace osprey

