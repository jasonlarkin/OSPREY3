#pragma once

#include "../../include/next/IMove.h"
#include <vector>
#include <cstdint>

namespace next {
namespace osprey {

/**
 * Move: change rotamer assignment at one or more positions.
 */
class RotamerMove : public IMove {
public:
    // Single position move: position -> new_rotamer_index
    RotamerMove(size_t position, int new_rotamer);
    
    // Multi-position move: batch changes
    RotamerMove(std::vector<std::pair<size_t, int>> changes);
    
    ~RotamerMove() override = default;
    
    // IMove interface
    std::vector<size_t> affected_atoms() const override;
    std::vector<size_t> affected_residues() const override;
    
    // IMove interface
    std::string description() const override;

    // OSPREY-specific access
    const std::vector<std::pair<size_t, int>>& changes() const { return changes_; }
    
private:
    std::vector<std::pair<size_t, int>> changes_;  // (position, new_rotamer_index)
};

} // namespace osprey
} // namespace next
