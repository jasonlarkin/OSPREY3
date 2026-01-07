#pragma once

#include <vector>
#include <memory>
#include "../../include/next/ISystem.h"
#include "../../include/next/IEnergyTerm.h"
#include "../../include/next/IState.h"

namespace next {
namespace openmm {

// Forward declarations
class System;

/**
 * Adapter: OpenMM's System as ISystem.
 * 
 * Wraps OpenMM's System (topology, particles, forces).
 */
class SystemWrapper : public ISystem {
public:
    // Construct from OpenMM System
    explicit SystemWrapper(System* system);
    
    ~SystemWrapper() override;
    
    // ISystem interface
    size_t num_atoms() const override;
    size_t num_residues() const override;
    void add_term(std::unique_ptr<IEnergyTerm> term) override;
    size_t num_terms() const override;
    std::unique_ptr<IState> create_state() const override;
    
private:
    System* system_;  // Owned or borrowed? TBD
    std::vector<std::unique_ptr<IEnergyTerm>> terms_;
};

} // namespace openmm
} // namespace next
