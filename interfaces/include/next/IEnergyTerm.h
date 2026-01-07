#pragma once

#include <memory>
#include <limits>

namespace next {

// Forward declarations
class IState;
class IMove;

/**
 * Abstract energy term (single contribution to total energy).
 * 
 * Pattern: Composable energy terms
 * - Matches OpenMM: Force objects
 * - Matches Rosetta: EnergyMethod objects
 * - Matches LAMMPS: Pair/Fix/Compute objects
 * 
 * Terms declare capabilities (delta_energy, lower_bound) for algorithm efficiency.
 */
class IEnergyTerm {
public:
    virtual ~IEnergyTerm() = default;
    
    // Main evaluation
    virtual double evaluate(const IState& state) = 0;
    
    // Capability flags
    virtual bool supports_delta_energy() const { return false; }
    virtual bool supports_lower_bound() const { return false; }
    virtual bool supports_gradients() const { return false; }
    
    // Optional: incremental evaluation (for A*, search efficiency)
    // Returns energy change from applying move to state
    virtual double delta_energy(const IState& old_state, const IState& new_state, const IMove& move) = 0;

    // Optional: lower bound for partial states (for branch-and-bound)
    virtual double lower_bound(const IState& partial_state) = 0;
};

} // namespace next
