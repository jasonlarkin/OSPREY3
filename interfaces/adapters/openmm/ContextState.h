#pragma once

#include <vector>
#include <memory>
#include "../../include/next/IState.h"
#include "../../include/next/ISystem.h"

namespace next {
namespace openmm {

// Forward declarations
class Context;

/**
 * Adapter: OpenMM's Context as IState.
 * 
 * Wraps OpenMM's Context (runtime state: positions, velocities, parameters).
 */
class ContextState : public IState {
public:
    // Construct from OpenMM Context and bind to ISystem
    ContextState(const ISystem& system, Context* context);
    
    ~ContextState() override;
    
    // IState interface
    const ISystem& system() const override;
    size_t num_atoms() const override;
    void set_coordinates(const std::vector<double>& coords) override;
    std::vector<double> get_coordinates() const override;
    std::unique_ptr<IState> clone() const override;
    
private:
    const ISystem& system_;
    Context* context_;  // Owned or borrowed? TBD
};

} // namespace openmm
} // namespace next
