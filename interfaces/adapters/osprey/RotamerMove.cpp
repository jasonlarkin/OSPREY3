#include <sstream>
#include "RotamerMove.h"

namespace next {
namespace osprey {

RotamerMove::RotamerMove(size_t position, int new_rotamer) {
    changes_.emplace_back(position, new_rotamer);
}

RotamerMove::RotamerMove(std::vector<std::pair<size_t, int>> changes)
    : changes_(std::move(changes)) {
}

std::vector<size_t> RotamerMove::affected_atoms() const {
    // In OSPREY, each position corresponds to a residue
    // For atoms, need to know which atoms are affected by rotamer changes
    // For now, return empty (atoms would be determined by residue)

    std::vector<size_t> atoms;
    // In real implementation:
    // for each changed position, add all atoms in that rotamer

    return atoms;
}

std::vector<size_t> RotamerMove::affected_residues() const {
    std::vector<size_t> residues;
    residues.reserve(changes_.size());

    for (const auto& change : changes_) {
        residues.push_back(change.first);  // position = residue index
    }

    return residues;
}

std::string RotamerMove::description() const {
    if (changes_.size() == 1) {
        const auto& change = changes_[0];
        return "Change rotamer at position " + std::to_string(change.first) +
               " to " + std::to_string(change.second);
    } else {
        return "Change " + std::to_string(changes_.size()) + " rotamer positions";
    }
}

} // namespace osprey
} // namespace next