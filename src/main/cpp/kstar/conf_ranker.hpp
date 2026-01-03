#pragma once

#include <algorithm>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "conf_index.hpp"
#include "energy_matrix.hpp"

namespace osprey::kstar {

// Port of OSPREY Java:
// - edu.duke.cs.osprey.astar.conf.ConfRanker
//
// Goal: Count how many conformations have energy <= queryScore, using branch-and-bound
// with min/max subtree energy bounds derived from an EnergyMatrix.
//
// IMPORTANT: OSPREY's Java implementation iterates RC IDs from `RCs.get(pos)`, which may be
// a subset and/or not equal to [0, emat.getNumConfAtPos(pos)).
// To match Java verbatim, this C++ port must accept the RC ID lists per position.
class ConfRanker final {
public:
    // Java uses BigInteger for ranks/counts. For our current verbatim ConfRanker tests
    // the values fit in uint64_t (e.g. 21,039,231), so avoid adding Boost as a dependency.
    // If/when we need arbitrary precision, prefer GMP/MPFR (already planned).
    using BigInt = std::uint64_t;

    ConfRanker(const EnergyMatrix<double>& emat, std::vector<std::vector<int32_t>> rcs_at_pos)
        : emat_(emat)
        , num_positions_(emat.getNumPositions())
        , rcs_at_pos_(std::move(rcs_at_pos))
        , num_rcs_per_pos_(static_cast<size_t>(emat.getNumPositions()))
        , conf_index_(num_positions_) {
        if (static_cast<int32_t>(rcs_at_pos_.size()) != num_positions_) {
            throw std::invalid_argument("rcs_at_pos size mismatch");
        }
        for (int32_t pos = 0; pos < num_positions_; pos++) {
            num_rcs_per_pos_[static_cast<size_t>(pos)] = rcs_at_pos_[static_cast<size_t>(pos)].size();
        }
        precomputeUndefinedMinPairEnergies();
        precomputeUndefinedMaxPairEnergies();
    }

    [[nodiscard]] BigInt getNumConfsAtMost(double queryScore) {
        Progress progress(totalConfs());
        Node root(num_positions_);
        root.gscore = emat_.getConstTerm();
        // compute bounds at root
        indexNode(root);
        root.minHScore = computeMinH(conf_index_);
        root.maxHScore = computeMaxH(conf_index_);

        numConfsAtMost(root, queryScore, progress);
        return progress.below;
    }

private:
    static constexpr int32_t Unassigned = -1;

    struct Progress {
        BigInt total;
        BigInt below = 0;
        BigInt above = 0;

        explicit Progress(BigInt total_) : total(std::move(total_)) {}

        void incrementBelow() { below += 1; }
        void incrementAbove() { above += 1; }
        void incrementBelow(const BigInt& val) { below += val; }
        void incrementAbove(const BigInt& val) { above += val; }
    };

    struct Node {
        double gscore = std::numeric_limits<double>::quiet_NaN();
        double minHScore = std::numeric_limits<double>::quiet_NaN();
        double maxHScore = std::numeric_limits<double>::quiet_NaN();
        std::vector<int32_t> assignments;
        int32_t pos = Unassigned;
        int32_t rc = Unassigned;

        explicit Node(int32_t size) : assignments(static_cast<size_t>(size), Unassigned) {}

        [[nodiscard]] Node assign(int32_t pos_, int32_t rc_) const {
            Node node(static_cast<int32_t>(assignments.size()));
            node.pos = pos_;
            node.rc = rc_;
            node.assignments = assignments;
            node.assignments[static_cast<size_t>(pos_)] = rc_;
            return node;
        }

        [[nodiscard]] double getMinScore() const { return gscore + minHScore; }
        [[nodiscard]] double getMaxScore() const { return gscore + maxHScore; }

        [[nodiscard]] BigInt getNumConformations(std::span<const size_t> numRcsPerPos) const {
            BigInt num = 1;
            for (size_t posi = 0; posi < assignments.size(); posi++) {
                if (assignments[posi] == Unassigned) {
                    const BigInt factor = static_cast<BigInt>(numRcsPerPos[posi]);
                    const __uint128_t prod =
                        static_cast<__uint128_t>(num) * static_cast<__uint128_t>(factor);
                    if (prod > std::numeric_limits<BigInt>::max()) {
                        throw std::overflow_error("ConfRanker getNumConformations overflow (need big integer)");
                    }
                    num = static_cast<BigInt>(prod);
                }
            }
            return num;
        }
    };

    const EnergyMatrix<double>& emat_;
    int32_t num_positions_;
    std::vector<std::vector<int32_t>> rcs_at_pos_;
    std::vector<size_t> num_rcs_per_pos_;
    ConfIndex conf_index_;

    // undefined_min_[pos1][rc1][pos2] where pos2 < pos1: min over rc2 pairwise(pos1,rc1,pos2,rc2)
    std::vector<std::vector<std::vector<double>>> undefined_min_;
    // undefined_max_[pos1][rc1][pos2] where pos2 < pos1: max over rc2 pairwise(pos1,rc1,pos2,rc2)
    std::vector<std::vector<std::vector<double>>> undefined_max_;

    [[nodiscard]] double getPairwiseSym(int32_t pos1, int32_t rc1, int32_t pos2, int32_t rc2) const {
        if (pos1 == pos2) {
            return 0.0;
        }
        if (pos1 > pos2) {
            return emat_.getPairwise(pos1, rc1, pos2, rc2);
        }
        return emat_.getPairwise(pos2, rc2, pos1, rc1);
    }

    [[nodiscard]] BigInt totalConfs() const {
        BigInt total = 1;
        for (int32_t pos = 0; pos < num_positions_; pos++) {
            const BigInt factor = static_cast<BigInt>(num_rcs_per_pos_[static_cast<size_t>(pos)]);
            const __uint128_t prod =
                static_cast<__uint128_t>(total) * static_cast<__uint128_t>(factor);
            if (prod > std::numeric_limits<BigInt>::max()) {
                throw std::overflow_error("ConfRanker totalConfs overflow (need big integer)");
            }
            total = static_cast<BigInt>(prod);
        }
        return total;
    }

    void precomputeUndefinedMinPairEnergies() {
        undefined_min_.resize(num_positions_);
        for (int32_t pos1 = 0; pos1 < num_positions_; ++pos1) {
            const auto& rcs1 = rcs_at_pos_[static_cast<size_t>(pos1)];
            undefined_min_[pos1].resize(rcs1.size());
            for (size_t rci1 = 0; rci1 < rcs1.size(); ++rci1) {
                const int32_t rc1 = rcs1[rci1];
                undefined_min_[pos1][rci1].assign(static_cast<size_t>(num_positions_), 0.0);
                for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                    double min_energy = std::numeric_limits<double>::infinity();
                    for (int32_t rc2 : rcs_at_pos_[static_cast<size_t>(pos2)]) {
                        min_energy = std::min(min_energy, emat_.getPairwise(pos1, rc1, pos2, rc2));
                    }
                    undefined_min_[pos1][static_cast<size_t>(rci1)][static_cast<size_t>(pos2)] = min_energy;
                }
            }
        }
    }

    void precomputeUndefinedMaxPairEnergies() {
        undefined_max_.resize(num_positions_);
        for (int32_t pos1 = 0; pos1 < num_positions_; ++pos1) {
            const auto& rcs1 = rcs_at_pos_[static_cast<size_t>(pos1)];
            undefined_max_[pos1].resize(rcs1.size());
            for (size_t rci1 = 0; rci1 < rcs1.size(); ++rci1) {
                const int32_t rc1 = rcs1[rci1];
                undefined_max_[pos1][rci1].assign(static_cast<size_t>(num_positions_), 0.0);
                for (int32_t pos2 = 0; pos2 < pos1; ++pos2) {
                    double max_energy = -std::numeric_limits<double>::infinity();
                    for (int32_t rc2 : rcs_at_pos_[static_cast<size_t>(pos2)]) {
                        max_energy = std::max(max_energy, emat_.getPairwise(pos1, rc1, pos2, rc2));
                    }
                    undefined_max_[pos1][static_cast<size_t>(rci1)][static_cast<size_t>(pos2)] = max_energy;
                }
            }
        }
    }

    void indexNode(const Node& node) {
        conf_index_.numDefined = 0;
        for (int32_t pos = 0; pos < num_positions_; pos++) {
            const int32_t rc = node.assignments[static_cast<size_t>(pos)];
            if (rc != Unassigned) {
                conf_index_.definedPos[static_cast<size_t>(conf_index_.numDefined)] = pos;
                conf_index_.definedRCs[static_cast<size_t>(conf_index_.numDefined)] = rc;
                conf_index_.numDefined++;
            }
        }
        conf_index_.sortDefined();
        conf_index_.updateUndefined();
    }

    [[nodiscard]] double gscoreDifferential(double parentG, const ConfIndex& parentIndex, int32_t pos, int32_t rc) const {
        double g = parentG;
        g += emat_.getOneBody(pos, rc);
        for (int32_t i = 0; i < parentIndex.numDefined; i++) {
            const int32_t pos2 = parentIndex.definedPos[static_cast<size_t>(i)];
            const int32_t rc2 = parentIndex.definedRCs[static_cast<size_t>(i)];
            g += getPairwiseSym(pos2, rc2, pos, rc);
        }
        return g;
    }

    [[nodiscard]] ConfIndex childIndex(const ConfIndex& parentIndex, int32_t pos, int32_t rc) const {
        ConfIndex child(parentIndex);
        child.assignInPlace(pos, rc);
        // assignInPlace recomputes undefined
        (void)rc;
        return child;
    }

    void calcCachedEnergiesMin(const ConfIndex& index, std::vector<std::vector<double>>& cached) const {
        // Mirrors TraditionalPairwiseHScorer.calcCachedEnergies (Minimize)
        for (int32_t ui = 0; ui < index.numUndefined; ui++) {
            const int32_t pos1 = index.undefinedPos[static_cast<size_t>(ui)];
            const auto& rcs1 = rcs_at_pos_[static_cast<size_t>(pos1)];
            cached[pos1].assign(rcs1.size(), 0.0);
 
            for (size_t rci1 = 0; rci1 < rcs1.size(); rci1++) {
                const int32_t rc1 = rcs1[rci1];
                double e = emat_.getOneBody(pos1, rc1);

                // add defined energies
                for (int32_t di = 0; di < index.numDefined; di++) {
                    const int32_t pos2 = index.definedPos[static_cast<size_t>(di)];
                    const int32_t rc2 = index.definedRCs[static_cast<size_t>(di)];
                    e += getPairwiseSym(pos1, rc1, pos2, rc2);
                }

                // add undefined energies (pos2 < pos1)
                for (int32_t uj = 0; uj < index.numUndefined; uj++) {
                    const int32_t pos2 = index.undefinedPos[static_cast<size_t>(uj)];
                    if (pos2 < pos1) {
                        e += undefined_min_[pos1][rci1][static_cast<size_t>(pos2)];
                    }
                }

                cached[pos1][rci1] = e;
            }
        }
    }

    void calcCachedEnergiesNeg(const ConfIndex& index, std::vector<std::vector<double>>& cachedNeg) const {
        // Mirrors TraditionalPairwiseHScorer.calcCachedEnergies on NegatedEnergyMatrix (Minimize)
        for (int32_t ui = 0; ui < index.numUndefined; ui++) {
            const int32_t pos1 = index.undefinedPos[static_cast<size_t>(ui)];
            const auto& rcs1 = rcs_at_pos_[static_cast<size_t>(pos1)];
            cachedNeg[pos1].assign(rcs1.size(), 0.0);
 
            for (size_t rci1 = 0; rci1 < rcs1.size(); rci1++) {
                const int32_t rc1 = rcs1[rci1];
                double e = -emat_.getOneBody(pos1, rc1);

                // add defined energies (negated)
                for (int32_t di = 0; di < index.numDefined; di++) {
                    const int32_t pos2 = index.definedPos[static_cast<size_t>(di)];
                    const int32_t rc2 = index.definedRCs[static_cast<size_t>(di)];
                    e += -getPairwiseSym(pos1, rc1, pos2, rc2);
                }

                // add undefined energies (negated, pos2 < pos1)
                for (int32_t uj = 0; uj < index.numUndefined; uj++) {
                    const int32_t pos2 = index.undefinedPos[static_cast<size_t>(uj)];
                    if (pos2 < pos1) {
                        e += -undefined_max_[pos1][rci1][static_cast<size_t>(pos2)];
                    }
                }

                cachedNeg[pos1][rci1] = e;
            }
        }
    }

    [[nodiscard]] double computeMinH(const ConfIndex& index) const {
        if (index.numUndefined <= 0) {
            return 0.0;
        }
        std::vector<std::vector<double>> cached(num_positions_);
        calcCachedEnergiesMin(index, cached);

        double h = 0.0;
        for (int32_t ui = 0; ui < index.numUndefined; ui++) {
            const int32_t pos = index.undefinedPos[static_cast<size_t>(ui)];
            double opt = std::numeric_limits<double>::infinity();
            for (double v : cached[pos]) {
                opt = std::min(opt, v);
            }
            h += opt;
        }
        return h;
    }

    [[nodiscard]] double computeMaxH(const ConfIndex& index) const {
        if (index.numUndefined <= 0) {
            return 0.0;
        }
        std::vector<std::vector<double>> cachedNeg(num_positions_);
        calcCachedEnergiesNeg(index, cachedNeg);

        double hneg = 0.0;
        for (int32_t ui = 0; ui < index.numUndefined; ui++) {
            const int32_t pos = index.undefinedPos[static_cast<size_t>(ui)];
            double opt = std::numeric_limits<double>::infinity();
            for (double v : cachedNeg[pos]) {
                opt = std::min(opt, v);
            }
            hneg += opt;
        }
        return -hneg;
    }

    [[nodiscard]] double computeMinHDifferential(
        const ConfIndex& parentIndex,
        const std::vector<std::vector<double>>& cachedMin,
        int32_t nextPos,
        int32_t nextRc
    ) const {
        double h = 0.0;
        for (int32_t ui = 0; ui < parentIndex.numUndefined; ui++) {
            const int32_t pos = parentIndex.undefinedPos[static_cast<size_t>(ui)];
            if (pos == nextPos) {
                continue;
            }

            double opt = std::numeric_limits<double>::infinity();
            const auto& rcsAtPos = rcs_at_pos_[static_cast<size_t>(pos)];
            for (size_t rci = 0; rci < rcsAtPos.size(); rci++) {
                const int32_t rc = rcsAtPos[rci];
                double e = cachedMin[pos][rci];

                // subtract undefined contribution (only if pos > nextPos)
                if (pos > nextPos) {
                    e -= undefined_min_[pos][rci][static_cast<size_t>(nextPos)];
                }

                // add defined contribution
                e += getPairwiseSym(pos, rc, nextPos, nextRc);

                opt = std::min(opt, e);
            }

            h += opt;
        }
        return h;
    }

    [[nodiscard]] double computeMaxHDifferential(
        const ConfIndex& parentIndex,
        const std::vector<std::vector<double>>& cachedNeg,
        int32_t nextPos,
        int32_t nextRc
    ) const {
        // compute negated h-score via differential, then negate to return maxH
        double hneg = 0.0;
        for (int32_t ui = 0; ui < parentIndex.numUndefined; ui++) {
            const int32_t pos = parentIndex.undefinedPos[static_cast<size_t>(ui)];
            if (pos == nextPos) {
                continue;
            }

            double opt = std::numeric_limits<double>::infinity();
            const auto& rcsAtPos = rcs_at_pos_[static_cast<size_t>(pos)];
            for (size_t rci = 0; rci < rcsAtPos.size(); rci++) {
                const int32_t rc = rcsAtPos[rci];
                double e = cachedNeg[pos][rci];

                // subtract undefined contribution (pos > nextPos) for NEGATED energies:
                // undefinedNeg = -undefined_max, so subtracting it adds undefined_max
                if (pos > nextPos) {
                    e -= (-undefined_max_[pos][rci][static_cast<size_t>(nextPos)]);
                }

                // add defined contribution (negated pairwise)
                e += -getPairwiseSym(pos, rc, nextPos, nextRc);

                opt = std::min(opt, e);
            }

            hneg += opt;
        }
        return -hneg;
    }

    void numConfsAtMost(Node& node, double queryScore, Progress& progress) {
        indexNode(node);
        if (conf_index_.numUndefined <= 0) {
            // fully assigned node, compare its exact score
            if (node.gscore <= queryScore) {
                progress.incrementBelow();
            } else {
                progress.incrementAbove();
            }
            return;
        }

        if (conf_index_.numUndefined == 1) {
            countLeaves(node, queryScore, progress);
        } else {
            countBranches(node, queryScore, progress);
        }
    }

    void countLeaves(const Node& node, double queryScore, Progress& progress) {
        const int32_t pos = conf_index_.undefinedPos[0];
        for (int32_t rc : rcs_at_pos_[static_cast<size_t>(pos)]) {
            const double score = gscoreDifferential(node.gscore, conf_index_, pos, rc);
            if (score <= queryScore) {
                progress.incrementBelow();
            } else {
                progress.incrementAbove();
            }
        }
    }

    void countBranches(const Node& node, double queryScore, Progress& progress) {
        // Precompute cached energies at parent node to match OSPREY's differential scoring order-of-ops.
        std::vector<std::vector<double>> cachedMin(num_positions_);
        std::vector<std::vector<double>> cachedNeg(num_positions_);
        calcCachedEnergiesMin(conf_index_, cachedMin);
        calcCachedEnergiesNeg(conf_index_, cachedNeg);

        std::vector<Node> childNodes;
        childNodes.reserve(4096);

        double bestPosScore = -std::numeric_limits<double>::infinity();
        int32_t bestPos = -1;

        // try each candidate position under the current node
        for (int32_t ui = 0; ui < conf_index_.numUndefined; ui++) {
            const int32_t pos = conf_index_.undefinedPos[static_cast<size_t>(ui)];
            const int32_t numRCs = static_cast<int32_t>(rcs_at_pos_[static_cast<size_t>(pos)].size());

            int32_t numSubTreesPruned = 0;
            for (int32_t rc : rcs_at_pos_[static_cast<size_t>(pos)]) {
                Node child = node.assign(pos, rc);

                // compute bounds for this child
                child.gscore = gscoreDifferential(node.gscore, conf_index_, pos, rc);
                child.minHScore = computeMinHDifferential(conf_index_, cachedMin, pos, rc);

                if (child.getMinScore() > queryScore) {
                    numSubTreesPruned++;
                    childNodes.push_back(std::move(child));
                    continue;
                }

                child.maxHScore = computeMaxHDifferential(conf_index_, cachedNeg, pos, rc);

                if (child.getMinScore() > queryScore) {
                    numSubTreesPruned++;
                } else if (child.getMaxScore() <= queryScore) {
                    numSubTreesPruned++;
                }

                childNodes.push_back(std::move(child));
            }

            const double posScore = static_cast<double>(numSubTreesPruned) / static_cast<double>(numRCs);
            if (posScore > bestPosScore) {
                bestPosScore = posScore;
                bestPos = pos;
            }
        }

        if (bestPos < 0) {
            throw std::runtime_error("ConfRanker: failed to choose bestPos");
        }

        // prune children under the best position, and discard other positions
        std::vector<Node> kept;
        kept.reserve(childNodes.size());
        for (auto& child : childNodes) {
            if (child.pos != bestPos) {
                continue;
            }

            if (child.getMinScore() > queryScore) {
                progress.incrementAbove(child.getNumConformations(num_rcs_per_pos_));
                continue;
            }

            if (child.getMaxScore() <= queryScore) {
                progress.incrementBelow(child.getNumConformations(num_rcs_per_pos_));
                continue;
            }

            kept.push_back(std::move(child));
        }

        // recurse on remaining children
        for (auto& child : kept) {
            numConfsAtMost(child, queryScore, progress);
        }
    }
};

} // namespace osprey::kstar


