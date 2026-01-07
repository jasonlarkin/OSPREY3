#pragma once

#include "IConformation.h"
#include <memory>
#include <vector>
#include <limits>

namespace next {

// Forward declarations
class ISearchProblem;

/**
 * Search result.
 */
struct SearchResult {
    std::unique_ptr<IConformation> best_conformation;
    double best_cost;
    size_t nodes_evaluated;
    size_t nodes_expanded;
    
    SearchResult() : best_cost(std::numeric_limits<double>::max()),
                     nodes_evaluated(0), nodes_expanded(0) {}
};

/**
 * Abstract search algorithm.
 * 
 * Pattern: Generic search over ISearchProblem
 * - Matches OSPREY: A* implementation
 * - Can support: branch-and-bound, beam search, MCTS, etc.
 */
class ISearchAlgorithm {
public:
    virtual ~ISearchAlgorithm() = default;
    
    // Main search entry point
    virtual SearchResult search(ISearchProblem& problem) = 0;
    
    // Algorithm metadata
    virtual const char* name() const = 0;
    virtual bool supports_partial_states() const { return false; }
};

} // namespace next
