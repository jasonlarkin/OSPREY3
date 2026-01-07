#pragma once

#include <vector>
#include <memory>
#include "../../include/next/INeighborhood.h"
#include "../../include/next/IConformation.h"
#include "../../include/next/IMove.h"

namespace next {
namespace openbabel {

// Forward declarations
class OBConformerSearch;

/**
 * Adapter: Open Babel's OBConformerSearch as INeighborhood.
 * 
 * Generates conformer moves using Open Babel's conformer search.
 */
class OBConformerNeighborhood : public INeighborhood {
public:
    // Construct from Open Babel OBConformerSearch
    explicit OBConformerNeighborhood(OBConformerSearch* conformer_search);
    
    ~OBConformerNeighborhood() override;
    
    // INeighborhood interface
    std::vector<std::unique_ptr<IMove>> generate_moves(const IConformation& conf) override;
    
private:
    OBConformerSearch* conformer_search_;  // Owned or borrowed? TBD
};

} // namespace openbabel
} // namespace next
