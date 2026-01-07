#include <algorithm>
#include <stdexcept>
#include <iostream>
#include "RotamerConformation.h"

namespace next {
namespace osprey {

RotamerConformation::RotamerConformation(std::vector<int> rotamer_assignments)
    : assignments_(std::move(rotamer_assignments)) {
    // Check for partial conformation (negative rotamer indices indicate unassigned)
    partial_ = std::any_of(assignments_.begin(), assignments_.end(),
                          [](int rot) { return rot < 0; });
}

std::unique_ptr<IState> RotamerConformation::to_state() const {
    // Convert rotamer assignment to Cartesian coordinates
    // In OSPREY: this would call the conformation building logic

    // For demo: create a simple state
    // Real implementation would:
    // 1. Use OSPREY's conformation building to generate coordinates
    // 2. Return an OSPREY-specific IState implementation

    std::cout << "RotamerConformation::to_state() - building coordinates for "
              << assignments_.size() << " positions\n";

    // Placeholder: return nullptr for now
    // Would return std::make_unique<OSPREYState>(built_coordinates);
    return nullptr;
}

void RotamerConformation::from_state(const IState& state) {
    // Extract rotamer assignment from coordinates
    // This is the inverse of to_state()

    // In OSPREY: use rotamer identification logic
    // For demo: leave assignments unchanged

    std::cout << "RotamerConformation::from_state() - extracting rotamers from coordinates\n";

    // Real implementation would analyze coordinates and set assignments_
}

bool RotamerConformation::operator==(const IConformation& other) const {
    // Compare rotamer assignments
    const auto* other_rotamer = dynamic_cast<const RotamerConformation*>(&other);
    if (!other_rotamer) {
        return false;
    }

    return assignments_ == other_rotamer->assignments_;
}

bool RotamerConformation::is_partial() const {
    return partial_;
}

} // namespace osprey
} // namespace next