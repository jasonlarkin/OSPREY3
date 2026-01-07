#pragma once

#include <memory>
#include <vector>

namespace next {

// Forward declarations
class IEnergyTerm;
class IState;

/**
 * Molecular system topology and composition.
 * 
 * Owns:
 * - Topology information (atoms, bonds, constraints)
 * - List of energy terms
 * 
 * Pattern: Separation of topology (ISystem) from runtime state (IState)
 * - Matches OpenMM: System (topology) + Context (state)
 * - Matches GROMACS: Topology + SimulationState
 * - Matches Rosetta: Pose contains both but conceptually separated
 */
class ISystem {
public:
    virtual ~ISystem() = default;
    
    // Topology access
    virtual size_t num_atoms() const = 0;
    virtual size_t num_residues() const = 0;
    
    // Term management
    virtual void add_term(std::unique_ptr<IEnergyTerm> term) = 0;
    virtual size_t num_terms() const = 0;
    
    // Create a runtime state bound to this system
    virtual std::unique_ptr<IState> create_state() const = 0;
};

} // namespace next
