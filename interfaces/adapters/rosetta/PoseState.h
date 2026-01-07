#pragma once

#include <vector>
#include <memory>
#include "../../include/next/IState.h"
#include "../../include/next/ISystem.h"

namespace next {
namespace rosetta {

// Forward declarations
class Pose;
namespace next {
class ISystem;
}

/**
 * Adapter: Rosetta's Pose as IState.
 * 
 * Wraps Rosetta's molecular structure state container.
 */
class PoseState : public IState {
public:
    // Construct from Rosetta Pose and bind to ISystem
    PoseState(const ISystem& system, Pose* pose);
    
    ~PoseState() override;
    
    // IState interface
    const ISystem& system() const override;
    size_t num_atoms() const override;
    void set_coordinates(const std::vector<double>& coords) override;
    std::vector<double> get_coordinates() const override;
    std::unique_ptr<IState> clone() const override;
    
private:
    const ISystem& system_;
    Pose* pose_;  // Owned or borrowed? TBD
};

} // namespace rosetta
} // namespace next
