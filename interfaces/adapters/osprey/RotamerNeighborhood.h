#pragma once

#include "../../include/next/INeighborhood.h"
#include "../../include/next/IConformation.h"
#include "RotamerConformation.h"
#include "RotamerMove.h"
#include <memory>
#include <vector>

namespace next {
namespace osprey {

// Forward declaration
class ConfSpace;  // OSPREY's conformation space definition

/**
 * Generate moves in rotamer space.
 * 
 * Produces single-position rotamer changes (for A*) or batch changes (for other algorithms).
 */
class RotamerNeighborhood : public INeighborhood {
public:
    // Construct from OSPREY ConfSpace
    explicit RotamerNeighborhood(ConfSpace* conf_space);
    
    ~RotamerNeighborhood() override = default;
    
    // INeighborhood interface
    std::vector<std::unique_ptr<IMove>> generate_moves(const IConformation& conf) override;
    
    // OSPREY-specific: generate moves at specific position
    std::vector<std::unique_ptr<IMove>> generate_moves_at_position(
        const RotamerConformation& conf, 
        size_t position
    ) const;
    
private:
    ConfSpace* conf_space_;
    
    // Helper: cast to RotamerConformation
    const RotamerConformation* as_rotamer_conf(const IConformation& conf) const;
};

} // namespace osprey
} // namespace next
