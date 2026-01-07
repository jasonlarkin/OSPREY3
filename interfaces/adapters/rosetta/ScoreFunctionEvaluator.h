#pragma once

#include <memory>
#include "../../include/next/IEnergyEvaluator.h"
#include "../../include/next/IState.h"
#include "../../include/next/IEnergyTerm.h"

namespace next {
namespace rosetta {

// Forward declarations - Rosetta types
class ScoreFunction;
class Pose;

/**
 * Adapter: Rosetta's ScoreFunction as IEnergyEvaluator.
 * 
 * Wraps Rosetta's weighted composition of EnergyMethod terms.
 * 
 * Note: IEnergyEvaluator is a concrete class, so we compose it rather than inherit.
 */
class ScoreFunctionEvaluator {
public:
    // Construct from Rosetta ScoreFunction
    explicit ScoreFunctionEvaluator(ScoreFunction* score_function);
    
    ~ScoreFunctionEvaluator();
    
    // IEnergyEvaluator-compatible interface
    double evaluate(const IState& state);
    
    bool supports_delta_energy() const;
    double delta_energy(const IState& old_state, const IState& new_state, const IMove& move);
    
    bool supports_lower_bound() const;
    double lower_bound(const IState& partial_state);
    
private:
    ScoreFunction* score_function_;  // Owned or borrowed? TBD
    
    // Helper: convert IState to Rosetta Pose
    Pose* to_pose(const IState& state) const;
};

} // namespace rosetta
} // namespace next
