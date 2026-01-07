#pragma once

#include <vector>
#include <memory>

namespace next {

// Forward declarations
class IState;

/**
 * Compact conformation representation (DOF assignments).
 * 
 * Not necessarily Cartesian coordinates - can be discrete (rotamer indices)
 * or continuous (angles, distances).
 * 
 * Pattern: Abstract state representation for search
 * - Matches OSPREY: rotamer assignment vector
 * - Matches Open Babel: RotorKey (torsion angle assignments)
 * - Matches Rosetta: Pose (but more abstract)
 */
class IConformation {
public:
    virtual ~IConformation() = default;
    
    // Convert to/from IState coordinates (if applicable)
    virtual std::unique_ptr<IState> to_state() const = 0;
    virtual void from_state(const IState& state) = 0;
    
    // Equality/hashing for search algorithms
    virtual bool operator==(const IConformation& other) const = 0;
    
    // Check if this is a partial state (for branch-and-bound)
    virtual bool is_partial() const { return false; }
};

} // namespace next
