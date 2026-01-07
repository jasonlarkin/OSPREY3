#pragma once

#include "../../include/next/IConformation.h"
#include "../../include/next/IState.h"
#include <vector>
#include <memory>

namespace next {
namespace osprey {

/**
 * OSPREY rotamer assignment as IConformation.
 * 
 * Represents a discrete conformation as rotamer choices at each position.
 */
class RotamerConformation : public IConformation {
public:
    // rotamer_assignments[i] = rotamer index at position i
    explicit RotamerConformation(std::vector<int> rotamer_assignments);
    
    ~RotamerConformation() override = default;
    
    // IConformation interface
    std::unique_ptr<IState> to_state() const override;
    void from_state(const IState& state) override;
    
    bool operator==(const IConformation& other) const override;
    
    bool is_partial() const override;
    
    // OSPREY-specific access
    const std::vector<int>& rotamer_assignments() const { return assignments_; }
    size_t num_positions() const { return assignments_.size(); }
    
private:
    std::vector<int> assignments_;  // rotamer index per position
    bool partial_;  // true if any position has unassigned rotamer (-1)
};

} // namespace osprey
} // namespace next
