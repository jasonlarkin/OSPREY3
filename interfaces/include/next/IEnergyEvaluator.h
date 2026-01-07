#pragma once

#include "IEnergyTerm.h"
#include "IState.h"
#include "IMove.h"
#include <memory>
#include <vector>

namespace next {

/**
 * Composed energy evaluator (sum of multiple terms).
 * 
 * Pattern: Aggregator of energy terms
 * - Matches OpenMM: System owns vector<Force*>, Platform evaluates
 * - Matches Rosetta: ScoreFunction owns weighted EnergyMethod list
 * - Matches LAMMPS: Modify orchestrates Pair/Fix/Compute lists
 */
class IEnergyEvaluator {
public:
    virtual ~IEnergyEvaluator() = default;
    
    // Term management
    void add_term(std::unique_ptr<IEnergyTerm> term) {
        terms_.push_back(std::move(term));
    }
    
    size_t num_terms() const { return terms_.size(); }
    
    // Main evaluation (sum of all terms)
    double evaluate(const IState& state) {
        double total = 0.0;
        for (const auto& term : terms_) {
            total += term->evaluate(state);
        }
        return total;
    }
    
    // Optional: incremental evaluation (if all terms support it)
    bool supports_delta_energy() const {
        for (const auto& term : terms_) {
            if (!term->supports_delta_energy()) return false;
        }
        return !terms_.empty();
    }
    
    double delta_energy(const IState& old_state, const IState& new_state, const IMove& move) {
        double delta = 0.0;
        for (const auto& term : terms_) {
            delta += term->delta_energy(old_state, new_state, move);
        }
        return delta;
    }
    
    // Optional: lower bound (if all terms support it)
    bool supports_lower_bound() const {
        for (const auto& term : terms_) {
            if (!term->supports_lower_bound()) return false;
        }
        return !terms_.empty();
    }
    
    double lower_bound(const IState& partial_state) {
        double bound = 0.0;
        for (const auto& term : terms_) {
            bound += term->lower_bound(partial_state);
        }
        return bound;
    }
    
private:
    std::vector<std::unique_ptr<IEnergyTerm>> terms_;
};

} // namespace next
