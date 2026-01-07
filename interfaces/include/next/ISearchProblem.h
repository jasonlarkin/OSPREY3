#pragma once

#include "IConformation.h"
#include "IEnergyEvaluator.h"
#include "INeighborhood.h"
#include "IState.h"
#include <memory>
#include <limits>

namespace next {

// Forward declarations
class INeighborhood;
class IEnergyEvaluator;

/**
 * Search problem definition.
 * 
 * Composes neighborhood generation + energy evaluation + goal predicate.
 * 
 * Pattern: Problem formulation for search algorithms
 * - Matches OSPREY: rotamer space + EnergyMatrix + goal (lowest energy)
 * - Matches Rosetta: Mover pipeline + ScoreFunction + acceptance criteria
 */
class ISearchProblem {
public:
    ISearchProblem(
        std::unique_ptr<INeighborhood> neighborhood,
        std::unique_ptr<IEnergyEvaluator> evaluator
    ) : neighborhood_(std::move(neighborhood)),
        evaluator_(std::move(evaluator))
    {}
    
    virtual ~ISearchProblem() = default;
    
    // Objective function
    double cost(const IConformation& conf) {
        auto state = conf.to_state();
        if (!state) {
            return std::numeric_limits<double>::max();
        }
        return evaluator_->evaluate(*state);
    }
    
    // Neighborhood access
    INeighborhood& neighborhood() { return *neighborhood_; }
    IEnergyEvaluator& evaluator() { return *evaluator_; }
    
    // Goal predicate (e.g., energy below threshold, all DOFs assigned)
    virtual bool is_goal(const IConformation& conf) const = 0;
    
private:
    std::unique_ptr<INeighborhood> neighborhood_;
    std::unique_ptr<IEnergyEvaluator> evaluator_;
};

} // namespace next
