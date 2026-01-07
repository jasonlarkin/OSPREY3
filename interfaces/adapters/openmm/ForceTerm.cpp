#include <stdexcept>
#include <iostream>
#include "../../include/next/IMove.h"
#include "ForceTerm.h"

namespace next {
namespace openmm {

// Placeholder for OpenMM's Force
class Force {};

ForceTerm::ForceTerm(Force* force)
    : force_(force) {
    if (!force_) {
        throw std::invalid_argument("Force cannot be null");
    }
}

ForceTerm::~ForceTerm() {
    // Ownership TBD
}

double ForceTerm::evaluate(const IState& state) {
    // Would evaluate OpenMM Force
    return 0.0;
}

bool ForceTerm::supports_delta_energy() const {
    // OpenMM typically doesn't support efficient delta_energy
    return false;
}

double ForceTerm::delta_energy(const IState& old_state, const IState& new_state, const IMove& move) {
    throw std::runtime_error("delta_energy not supported for OpenMM Force");
}

bool ForceTerm::supports_lower_bound() const {
    return false;
}

double ForceTerm::lower_bound(const IState& partial_state) {
    throw std::runtime_error("lower_bound not supported for OpenMM Force");
}

} // namespace openmm
} // namespace next
