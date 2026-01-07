#pragma once

#include <memory>
#include "../../include/next/IEnergyTerm.h"
#include "../../include/next/IState.h"
#include "../../include/next/IMove.h"

namespace next {
namespace openmm {

// Forward declarations
class Force;

/**
 * Adapter: OpenMM's Force as IEnergyTerm.
 * 
 * Wraps OpenMM's Force objects (HarmonicBondForce, NonbondedForce, etc.).
 */
class ForceTerm : public IEnergyTerm {
public:
    // Construct from OpenMM Force
    explicit ForceTerm(Force* force);
    
    ~ForceTerm() override;
    
    // IEnergyTerm interface
    double evaluate(const IState& state) override;
    
    bool supports_delta_energy() const override;
    double delta_energy(const IState& old_state, const IState& new_state, const IMove& move) override;
    
    bool supports_lower_bound() const override;
    double lower_bound(const IState& partial_state) override;
    
private:
    Force* force_;  // Owned or borrowed? TBD
};

} // namespace openmm
} // namespace next
