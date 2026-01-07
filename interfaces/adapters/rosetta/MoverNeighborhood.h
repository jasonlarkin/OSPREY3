#pragma once

#include <vector>
#include <memory>
#include "../../include/next/INeighborhood.h"
#include "../../include/next/IConformation.h"
#include "../../include/next/IMove.h"

namespace next {
namespace rosetta {

// Forward declarations
class Mover;

/**
 * Adapter: Rosetta's Mover as INeighborhood.
 * 
 * Generates moves by applying Rosetta Movers to conformations.
 */
class MoverNeighborhood : public INeighborhood {
public:
    // Construct from Rosetta Mover
    explicit MoverNeighborhood(Mover* mover);
    
    ~MoverNeighborhood() override;
    
    // INeighborhood interface
    std::vector<std::unique_ptr<IMove>> generate_moves(const IConformation& conf) override;
    
private:
    Mover* mover_;  // Owned or borrowed? TBD
};

} // namespace rosetta
} // namespace next
