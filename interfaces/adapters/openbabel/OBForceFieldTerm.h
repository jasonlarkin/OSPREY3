#pragma once

#include <memory>
#include "../../include/next/IEnergyTerm.h"
#include "../../include/next/IState.h"
#include "../../include/next/IMove.h"

namespace next {
namespace openbabel {

// Forward declarations
class OBForceField;
class OBMol;

/**
 * Adapter: Open Babel's OBForceField as IEnergyTerm.
 * 
 * Wraps Open Babel's force field evaluation.
 */
class OBForceFieldTerm : public IEnergyTerm {
public:
    // Construct from Open Babel OBForceField
    explicit OBForceFieldTerm(OBForceField* force_field);
    
    ~OBForceFieldTerm() override;
    
    // IEnergyTerm interface
    double evaluate(const IState& state) override;
    
    bool supports_delta_energy() const override;
    double delta_energy(const IState& old_state, const IState& new_state, const IMove& move) override;
    
    bool supports_lower_bound() const override;
    double lower_bound(const IState& partial_state) override;
    
private:
    OBForceField* force_field_;  // Owned or borrowed? TBD
    
    // Helper: convert IState to Open Babel OBMol
    OBMol* to_obmol(const IState& state) const;
};

} // namespace openbabel
} // namespace next
