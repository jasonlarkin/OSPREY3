#pragma once

#include <memory>
#include <vector>

namespace next {

// Forward declarations
class ISystem;

/**
 * Runtime molecular state (coordinates, velocities, cached data).
 * 
 * Binds to an ISystem and provides thread-safe state access.
 * 
 * Pattern: Runtime state separate from topology
 * - Matches OpenMM: Context (state) vs System (topology)
 * - Matches GROMACS: SimulationState vs Topology
 * - Matches HOOMD-blue: System (state) vs SystemDefinition (topology)
 * - Matches LAMMPS: Simulation state (implicit, owned by LAMMPS object) vs Modify (orchestrates)
 */
class IState {
public:
    virtual ~IState() = default;
    
    // System binding
    virtual const ISystem& system() const = 0;
    
    // Coordinate access
    virtual size_t num_atoms() const = 0;
    virtual void set_coordinates(const std::vector<double>& coords) = 0;  // x1,y1,z1, x2,y2,z2, ...
    virtual std::vector<double> get_coordinates() const = 0;
    
    // State snapshot/restore
    virtual std::unique_ptr<IState> clone() const = 0;
};

} // namespace next
