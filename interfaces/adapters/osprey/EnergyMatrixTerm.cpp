#include <stdexcept>
#include <iostream>
#include "EnergyMatrixTerm.h"

namespace next {
namespace osprey {

EnergyMatrixTerm::EnergyMatrixTerm(EnergyMatrix* emat)
    : emat_(emat) {
    if (!emat_) {
        throw std::invalid_argument("EnergyMatrix cannot be null");
    }
}

EnergyMatrixTerm::~EnergyMatrixTerm() {
    // Note: ownership semantics TBD - does this adapter own the EnergyMatrix?
    // For now, assume it's externally managed
}

double EnergyMatrixTerm::evaluate(const IState& state) {
    // Extract rotamer assignment from state
    auto assignment = extract_rotamer_assignment(state);

    // Call OSPREY EnergyMatrix to compute total energy
    // This would be a JNI call or direct C++ API call to OSPREY
    double energy = 0.0;

    // Placeholder: sum pairwise energies
    // In real implementation:
    // energy = emat_->calcEnergy(assignment);

    std::cout << "EnergyMatrixTerm::evaluate() - computing energy for " << assignment.size() << " positions\n";

    // For demo: return a simple function of assignment
    for (size_t i = 0; i < assignment.size(); ++i) {
        if (assignment[i] >= 0) {
            energy += assignment[i] * 0.1;  // arbitrary energy calculation
        }
    }

    return energy;
}

double EnergyMatrixTerm::delta_energy(const IState& old_state, const IState& new_state, const IMove& move) {
    // OSPREY's strength: incremental energy calculation
    // Instead of full re-evaluation, only compute changed interactions

    // Extract assignments
    auto old_assignment = extract_rotamer_assignment(old_state);
    auto new_assignment = extract_rotamer_assignment(new_state);

    // Compute delta energy (new - old)
    // In real OSPREY: emat_->calcEnergyDelta(old_assignment, new_assignment, changed_positions)
    double delta = evaluate(new_state) - evaluate(old_state);

    std::cout << "EnergyMatrixTerm::delta_energy() - efficient incremental calculation\n";
    return delta;
}

double EnergyMatrixTerm::lower_bound(const IState& partial_state) {
    // Lower bound for A* search pruning
    // OSPREY can compute bounds for partial conformations

    auto assignment = extract_rotamer_assignment(partial_state);

    // Count assigned vs unassigned positions
    size_t assigned = 0;
    for (int rot : assignment) {
        if (rot >= 0) ++assigned;
    }

    // Lower bound: energy of assigned positions + minimum possible for unassigned
    double lower_bound = assigned * 0.05;  // conservative estimate

    std::cout << "EnergyMatrixTerm::lower_bound() - " << assigned << "/" << assignment.size() << " positions assigned\n";
    return lower_bound;
}

std::vector<int> EnergyMatrixTerm::extract_rotamer_assignment(const IState& state) const {
    // Extract rotamer assignment from IState
    // This would depend on how OSPREY conformation data is stored in IState
    // For now, return a dummy assignment

    // In real implementation, this would:
    // 1. Cast state to OSPREY-specific state type
    // 2. Extract the rotamer assignment vector

    size_t num_positions = 10;  // placeholder
    return std::vector<int>(num_positions, 0);  // all rotamer 0
}

} // namespace osprey
} // namespace next