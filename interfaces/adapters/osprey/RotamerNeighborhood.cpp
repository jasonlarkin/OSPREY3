#include <stdexcept>
#include <iostream>
#include "RotamerNeighborhood.h"

namespace next {
namespace osprey {

RotamerNeighborhood::RotamerNeighborhood(ConfSpace* conf_space)
    : conf_space_(conf_space) {
    if (!conf_space_) {
        throw std::invalid_argument("ConfSpace cannot be null");
    }
}

std::vector<std::unique_ptr<IMove>> RotamerNeighborhood::generate_moves(const IConformation& conf) {
    const auto* rotamer_conf = as_rotamer_conf(conf);
    if (!rotamer_conf) {
        throw std::invalid_argument("Conformation must be RotamerConformation");
    }

    // Generate all possible single-position moves
    std::vector<std::unique_ptr<IMove>> moves;

    for (size_t pos = 0; pos < rotamer_conf->num_positions(); ++pos) {
        // Get possible rotamers at this position
        // In OSPREY: conf_space_->getNumRotamers(pos)
        size_t num_rotamers = 3;  // placeholder

        int current_rotamer = rotamer_conf->rotamer_assignments()[pos];

        // Generate moves to all other rotamers
        for (int rot = 0; rot < static_cast<int>(num_rotamers); ++rot) {
            if (rot != current_rotamer) {
                moves.push_back(std::make_unique<RotamerMove>(pos, rot));
            }
        }
    }

    std::cout << "RotamerNeighborhood::generate_moves() - generated " << moves.size() << " moves\n";
    return moves;
}

std::vector<std::unique_ptr<IMove>> RotamerNeighborhood::generate_moves_at_position(
    const RotamerConformation& conf,
    size_t position) const {

    if (position >= conf.num_positions()) {
        return {};
    }

    std::vector<std::unique_ptr<IMove>> moves;

    // Get possible rotamers at this position
    size_t num_rotamers = 3;  // placeholder
    int current_rotamer = conf.rotamer_assignments()[position];

    // Generate moves to all other rotamers
    for (int rot = 0; rot < static_cast<int>(num_rotamers); ++rot) {
        if (rot != current_rotamer) {
            moves.push_back(std::make_unique<RotamerMove>(position, rot));
        }
    }

    return moves;
}

const RotamerConformation* RotamerNeighborhood::as_rotamer_conf(const IConformation& conf) const {
    return dynamic_cast<const RotamerConformation*>(&conf);
}

} // namespace osprey
} // namespace next