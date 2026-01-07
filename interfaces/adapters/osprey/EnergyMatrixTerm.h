#pragma once

#include "../../include/next/IEnergyTerm.h"
#include "../../include/next/IState.h"
#include "../../include/next/IMove.h"

namespace next {
namespace osprey {

// Forward declaration - OSPREY's EnergyMatrix (Java object via JNI or C++ wrapper)
class EnergyMatrix;  // TODO: JNI bridge or C++ wrapper

/**
 * Adapter: OSPREY's EnergyMatrix as IEnergyTerm.
 * 
 * Wraps OSPREY's pairwise precomputed energy matrix.
 * Supports delta_energy() and lower_bound() for efficient A* search.
 */
class EnergyMatrixTerm : public IEnergyTerm {
public:
    // Construct from OSPREY EnergyMatrix
    explicit EnergyMatrixTerm(EnergyMatrix* emat);
    
    ~EnergyMatrixTerm() override;
    
    // IEnergyTerm interface
    double evaluate(const IState& state) override;
    
    bool supports_delta_energy() const override { return true; }
    bool supports_lower_bound() const override { return true; }
    
    // Efficient incremental evaluation (OSPREY's strength)
    double delta_energy(const IState& old_state, const IState& new_state, const IMove& move) override;
    
    // Lower bound for partial conformations
    double lower_bound(const IState& partial_state) override;
    
private:
    EnergyMatrix* emat_;  // Owned or borrowed? TBD based on OSPREY integration
    
    // Helper: extract rotamer assignment from IState
    std::vector<int> extract_rotamer_assignment(const IState& state) const;
};

} // namespace osprey
} // namespace next
