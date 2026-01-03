#ifndef OSPREY_KSTAR_SEQUENCE_HPP
#define OSPREY_KSTAR_SEQUENCE_HPP

#include <vector>
#include <string>
#include <cstdint>

namespace osprey {

/**
 * Represents a protein sequence with residue assignments.
 * 
 * Each position has a residue conformation (RC) index.
 */
struct Sequence {
    std::vector<int32_t> residue_assignments;  // RC indices per position
    std::string string_representation;         // Human-readable format
    
    bool operator==(const Sequence& other) const {
        return residue_assignments == other.residue_assignments;
    }
    
    size_t hash() const {
        size_t h = 0;
        for (int32_t rc : residue_assignments) {
            h = h * 31 + static_cast<size_t>(rc);
        }
        return h;
    }
};

} // namespace osprey

// Hash function for std::unordered_map
namespace std {
template<>
struct hash<osprey::Sequence> {
    size_t operator()(const osprey::Sequence& seq) const {
        return seq.hash();
    }
};
} // namespace std

#endif // OSPREY_KSTAR_SEQUENCE_HPP

