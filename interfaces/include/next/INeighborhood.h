#pragma once

#include "IConformation.h"
#include "IMove.h"
#include <vector>
#include <memory>

namespace next {

/**
 * Neighborhood generator (moves from a state).
 * 
 * Pattern: State space navigation
 * - Matches Rosetta: Mover objects generate neighbor states
 * - Matches Open Babel: conformer search generates RotorKey variations
 */
class INeighborhood {
public:
    virtual ~INeighborhood() = default;
    
    // Generate moves from a conformation
    virtual std::vector<std::unique_ptr<IMove>> generate_moves(const IConformation& conf) = 0;
    
    // Optional: ordered/ranked moves (for best-first search)
    virtual bool supports_ordering() const { return false; }
    virtual void order_moves(std::vector<std::unique_ptr<IMove>>& moves) const {
        // Default: no ordering
    }
};

} // namespace next
