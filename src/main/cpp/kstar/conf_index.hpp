#pragma once

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace osprey::kstar {

// Port of OSPREY Java: edu.duke.cs.osprey.astar.conf.ConfIndex
// - Stores a partial assignment of positions -> RCs.
// - `definedPos/definedRCs` are kept sorted by position for indices [0, numDefined).
// - `undefinedPos` is valid for indices [0, numUndefined); callers may leave garbage past that.
struct ConfIndex final {
    int32_t numPos;

    int32_t numDefined = 0;
    std::vector<int32_t> definedPos;
    std::vector<int32_t> definedRCs;

    int32_t numUndefined = 0;
    std::vector<int32_t> undefinedPos;

    explicit ConfIndex(int32_t numPos_)
        : numPos(numPos_)
        , definedPos(static_cast<size_t>(numPos_))
        , definedRCs(static_cast<size_t>(numPos_))
        , undefinedPos(static_cast<size_t>(numPos_)) {}

    [[nodiscard]] bool isFullyDefined() const noexcept {
        return numDefined == numPos;
    }

    [[nodiscard]] int32_t findDefined(int32_t pos) const noexcept {
        for (int32_t i = 0; i < numDefined; i++) {
            if (definedPos[static_cast<size_t>(i)] == pos) {
                return i;
            }
        }
        return -1;
    }

    [[nodiscard]] bool isDefined(int32_t pos) const noexcept {
        return findDefined(pos) >= 0;
    }

    [[nodiscard]] int32_t findUndefined(int32_t pos) const noexcept {
        for (int32_t i = 0; i < numUndefined; i++) {
            if (undefinedPos[static_cast<size_t>(i)] == pos) {
                return i;
            }
        }
        return -1;
    }

    [[nodiscard]] bool isUndefined(int32_t pos) const noexcept {
        return findUndefined(pos) >= 0;
    }

    [[nodiscard]] ConfIndex assign(int32_t pos, int32_t rc) const {
        ConfIndex other(*this);
        other.assignInPlace(pos, rc);
        return other;
    }

    void assignInPlace(int32_t pos, int32_t rc) {
        // update defined side (binary search on the sorted prefix)
        auto begin = definedPos.begin();
        auto end = begin + numDefined;
        auto it = std::lower_bound(begin, end, pos);
        if (it != end && *it == pos) {
            throw std::invalid_argument("pos " + std::to_string(pos) + " already assigned");
        }

        const int32_t insertIndex = static_cast<int32_t>(std::distance(begin, it));
        for (int32_t i = numDefined; i > insertIndex; i--) {
            definedPos[static_cast<size_t>(i)] = definedPos[static_cast<size_t>(i - 1)];
            definedRCs[static_cast<size_t>(i)] = definedRCs[static_cast<size_t>(i - 1)];
        }

        definedPos[static_cast<size_t>(insertIndex)] = pos;
        definedRCs[static_cast<size_t>(insertIndex)] = rc;
        numDefined++;

        updateUndefined();
    }

    [[nodiscard]] ConfIndex unassign(int32_t pos) const {
        ConfIndex other(*this);
        other.unassignInPlace(pos);
        return other;
    }

    void unassignInPlace(int32_t pos) {
        auto begin = definedPos.begin();
        auto end = begin + numDefined;
        auto it = std::lower_bound(begin, end, pos);
        if (it == end || *it != pos) {
            throw std::invalid_argument("pos " + std::to_string(pos) + " not assigned");
        }

        const int32_t removeIndex = static_cast<int32_t>(std::distance(begin, it));
        numDefined--;
        for (int32_t i = removeIndex; i < numDefined; i++) {
            definedPos[static_cast<size_t>(i)] = definedPos[static_cast<size_t>(i + 1)];
            definedRCs[static_cast<size_t>(i)] = definedRCs[static_cast<size_t>(i + 1)];
        }

        updateUndefined();
    }

    // ensures assigned positions are sorted in increasing order (insertion sort)
    void sortDefined() noexcept {
        for (int32_t i = 1; i < numDefined; i++) {
            int32_t tempPos = definedPos[static_cast<size_t>(i)];
            int32_t tempRc = definedRCs[static_cast<size_t>(i)];

            int32_t j = i;
            for (; j >= 1 && tempPos < definedPos[static_cast<size_t>(j - 1)]; j--) {
                definedPos[static_cast<size_t>(j)] = definedPos[static_cast<size_t>(j - 1)];
                definedRCs[static_cast<size_t>(j)] = definedRCs[static_cast<size_t>(j - 1)];
            }
            definedPos[static_cast<size_t>(j)] = tempPos;
            definedRCs[static_cast<size_t>(j)] = tempRc;
        }
    }

    // Populates the unassigned positions based on what's not assigned.
    // Precondition: defined positions are sorted for [0, numDefined).
    void updateUndefined() noexcept {
        numUndefined = 0;

        int32_t i = 0;
        for (int32_t pos = 0; pos < numPos; pos++) {
            if (i < numDefined && pos == definedPos[static_cast<size_t>(i)]) {
                i++;
            } else {
                undefinedPos[static_cast<size_t>(numUndefined++)] = pos;
            }
        }
    }
};

} // namespace osprey::kstar


