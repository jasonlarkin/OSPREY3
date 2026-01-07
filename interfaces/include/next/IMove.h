#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace next {

/**
 * Move descriptor (state mutation).
 * 
 * Small value type describing a state change with locality metadata.
 * 
 * Pattern: Explicit move representation for search algorithms
 * - Matches Rosetta: Mover objects transform Pose states
 * - Matches Open Babel: RotorKey represents conformer changes
 */
class IMove {
public:
    virtual ~IMove() = default;
    
    // Locality: which atoms/residues are affected (for incremental evaluation)
    virtual std::vector<size_t> affected_atoms() const = 0;
    virtual std::vector<size_t> affected_residues() const = 0;

    // Human-readable description of the move
    virtual std::string description() const = 0;
};

} // namespace next
