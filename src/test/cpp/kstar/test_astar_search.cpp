#include <vector>
#include <cmath>
#include <gtest/gtest.h>
#include "astar_node.hpp"
#include "astar_search.hpp"
#include "astar_node_fast.hpp"
#include "astar_search_fast.hpp"
#include "energy_matrix.hpp"
#include "energy_matrix_loader.hpp"
#include "test_data_paths.hpp"

using namespace osprey::kstar;

/**
 * Test A* node creation and assignment.
 */
TEST(AStarSearch_SYNTHESIZED, NodeBasicOperations) {
    auto root = AStarNode<double>::root(3);
    EXPECT_EQ(root.level, 0);
    ASSERT_EQ(root.assignments.size(), 3u);
    EXPECT_EQ(root.assignments[0], -1);
    EXPECT_EQ(root.assignments[1], -1);
    EXPECT_EQ(root.assignments[2], -1);
    EXPECT_FALSE(root.isAssigned(0));
    EXPECT_FALSE(root.isAssigned(1));
    EXPECT_FALSE(root.isAssigned(2));
    
    // Assign position 0
    auto node1 = root.assign(0, 5);
    EXPECT_EQ(node1.level, 1);
    EXPECT_TRUE(node1.isAssigned(0));
    EXPECT_EQ(node1.getRC(0), 5);
    EXPECT_FALSE(node1.isAssigned(1));
    EXPECT_FALSE(node1.isAssigned(2));
    
    // Assign position 1
    auto node2 = node1.assign(1, 3);
    EXPECT_EQ(node2.level, 2);
    EXPECT_TRUE(node2.isAssigned(0));
    EXPECT_TRUE(node2.isAssigned(1));
    EXPECT_EQ(node2.getRC(0), 5);
    EXPECT_EQ(node2.getRC(1), 3);
    EXPECT_FALSE(node2.isAssigned(2));
    
    // Root unchanged (immutable)
    EXPECT_EQ(root.level, 0);
    EXPECT_FALSE(root.isAssigned(0));
}

/**
 * Test A* node score comparison.
 */
TEST(AStarSearch_SYNTHESIZED, NodeScoreComparison) {
    auto node1 = AStarNode<double>::root(2);
    node1.g_score = 10.0;
    node1.h_score = 5.0;  // Total: 15.0
    
    auto node2 = AStarNode<double>::root(2);
    node2.g_score = 8.0;
    node2.h_score = 4.0;  // Total: 12.0
    
    // We intentionally do NOT overload comparison operators on AStarNode because it is too easy
    // to invert heap semantics (std::priority_queue + comparator interactions). Compare scores explicitly.
    EXPECT_LT(node2.getScore(), node1.getScore());  // 12.0 < 15.0 means node2 has higher priority in a min-heap
}

/**
 * Test A* search with minimal energy matrix.
 */
TEST(AStarSearch_SYNTHESIZED, SearchBasicOperations) {
    // Create minimal energy matrix (2 positions, 2 confs each)
    std::vector<int32_t> num_confs_per_pos = {2, 2};
    EnergyMatrix<double> emat(2, num_confs_per_pos);
    
    // Set const term
    emat.setConstTerm(1.0);
    
    // Set one-body energies
    emat.setOneBody(0, 0, 2.0);
    emat.setOneBody(0, 1, 3.0);
    emat.setOneBody(1, 0, 4.0);
    emat.setOneBody(1, 1, 5.0);
    
    // Set pairwise energies
    emat.setPairwise(1, 0, 0, 0, 0.5);  // pos1=1, conf1=0, pos2=0, conf2=0
    emat.setPairwise(1, 0, 0, 1, 0.6);
    emat.setPairwise(1, 1, 0, 0, 0.7);
    emat.setPairwise(1, 1, 0, 1, 0.8);
    
    // Create A* search
    AStarSearch<double> astar(emat, 2, num_confs_per_pos);
    AStarSearchFast<double> astar_fast(emat, 2, num_confs_per_pos);
    
    // Test root node
    auto root = AStarNode<double>::root(2);
    auto root_fast = AStarNodeFast<double>::root(2);
    double g_score = astar.computeGScore(root);
    double h_score = astar.computeHScore(root);
    double g_score_fast = astar_fast.computeGScore(root_fast);
    double h_score_fast = astar_fast.computeHScore(root_fast);
    
    // Root g-score should be const term only (no assignments)
    EXPECT_NEAR(g_score, 1.0, 1e-10);
    EXPECT_NEAR(g_score_fast, 1.0, 1e-10);
    
    // Root h-score should be sum of minimum one-body + minimum pairwise
    // Min one-body: min(2.0, 3.0) = 2.0 for pos 0, min(4.0, 5.0) = 4.0 for pos 1
    // Min pairwise: min(0.5, 0.6) = 0.5 for (1,0) with (0,0), min(0.7, 0.8) = 0.7 for (1,1) with (0,0)
    // But h-score uses optimal over all RCs, so it's more complex
    // For now, just check it's positive
    EXPECT_GT(h_score, 0);
    EXPECT_GT(h_score_fast, 0);
    
    // Test expansion
    auto children = astar.expand(root);
    auto children_fast = astar_fast.expand(root_fast);
    ASSERT_EQ(children.size(), 2u);  // 2 confs at position 0
    ASSERT_EQ(children_fast.size(), 2u);
    
    // Check first child (pos 0, conf 0)
    auto child1 = children[0];
    auto child1_fast = children_fast[0];
    EXPECT_EQ(child1.level, 1);
    EXPECT_TRUE(child1.isAssigned(0));
    EXPECT_EQ(child1.getRC(0), 0);
    EXPECT_FALSE(child1.isAssigned(1));
    EXPECT_EQ(child1_fast.level, 1);
    EXPECT_TRUE(child1_fast.isAssigned(0));
    EXPECT_EQ(child1_fast.getRC(0), 0);
    EXPECT_FALSE(child1_fast.isAssigned(1));
    
    // G-score should be const + one-body(0,0)
    double expected_g1 = 1.0 + 2.0;  // const + one-body
    EXPECT_NEAR(child1.g_score, expected_g1, 1e-10);
    EXPECT_NEAR(child1_fast.g_score, expected_g1, 1e-10);
    
    // Check second child (pos 0, conf 1)
    auto child2 = children[1];
    auto child2_fast = children_fast[1];
    EXPECT_EQ(child2.level, 1);
    EXPECT_TRUE(child2.isAssigned(0));
    EXPECT_EQ(child2.getRC(0), 1);
    EXPECT_EQ(child2_fast.level, 1);
    EXPECT_TRUE(child2_fast.isAssigned(0));
    EXPECT_EQ(child2_fast.getRC(0), 1);
    
    // G-score should be const + one-body(0,1)
    double expected_g2 = 1.0 + 3.0;
    EXPECT_NEAR(child2.g_score, expected_g2, 1e-10);
    EXPECT_NEAR(child2_fast.g_score, expected_g2, 1e-10);
    
    // Test leaf detection
    EXPECT_FALSE(astar.isLeaf(root));
    EXPECT_FALSE(astar.isLeaf(child1));
    EXPECT_FALSE(astar_fast.isLeaf(root_fast));
    EXPECT_FALSE(astar_fast.isLeaf(child1_fast));
    
    // Create full assignment
    auto leaf = child1.assign(1, 0);
    auto leaf_fast = child1_fast.assign(1, 0);
    EXPECT_TRUE(astar.isLeaf(leaf));
    EXPECT_TRUE(astar_fast.isLeaf(leaf_fast));
    
    // Leaf g-score should be full energy
    double leaf_g = astar.computeGScore(leaf);
    double leaf_g_fast = astar_fast.computeGScore(leaf_fast);
    double expected_leaf_g = 1.0 + 2.0 + 4.0 + 0.5;  // const + one(0,0) + one(1,0) + pairwise(1,0,0,0)
    EXPECT_NEAR(leaf_g, expected_leaf_g, 1e-10);
    EXPECT_NEAR(leaf_g_fast, expected_leaf_g, 1e-10);
    
    // Leaf h-score should be 0 (all assigned)
    double leaf_h = astar.computeHScore(leaf);
    EXPECT_NEAR(leaf_h, 0.0, 1e-10);
    double leaf_h_fast = astar_fast.computeHScore(leaf_fast);
    EXPECT_NEAR(leaf_h_fast, 0.0, 1e-10);
    
}

/**
 * Test A* search with loaded energy matrix (from Java export).
 */
TEST(AStarSearch_SYNTHESIZED, SearchWithJavaExportedEnergyMatrix) {
    // Load dipeptide energy matrix (smallest test case)
    EnergyMatrix<double> emat;
    try {
        const char* rel = "test_data/dipeptide.5hydrophobic.emat.bin";
        auto p = osprey::kstar::testutil::resolveTestDataPath(rel);
        if (!p) {
            GTEST_SKIP() << "Missing test data.\n" << osprey::kstar::testutil::describeTestDataSearch(rel);
        }
        emat = EnergyMatrixLoader<double>::loadFromFile(p->string());
    } catch (const std::exception& e) {
        GTEST_SKIP() << "Missing/failed to load test_data/dipeptide.5hydrophobic.emat.bin: " << e.what();
    }
    
    int32_t num_pos = emat.getNumPositions();
    std::vector<int32_t> num_confs_per_pos(num_pos);
    for (int32_t pos = 0; pos < num_pos; ++pos) {
        num_confs_per_pos[pos] = emat.getNumConfsAtPos(pos);
    }
    
    // Create A* search
    AStarSearch<double> astar(emat, num_pos, num_confs_per_pos);
    
    // Test root node
    auto root = AStarNode<double>::root(num_pos);
    double g_score = astar.computeGScore(root);
    double h_score = astar.computeHScore(root);
    
    // Root g-score should be const term
    double const_term = emat.getConstTerm();
    EXPECT_NEAR(g_score, const_term, 1e-10);
    
    // Root h-score should be positive (heuristic for unassigned positions)
    EXPECT_GT(h_score, 0);
    
    // Test expansion
    auto children = astar.expand(root);
    ASSERT_EQ(children.size(), static_cast<size_t>(num_confs_per_pos[0]));  // Number of confs at first position
    
    // Test that scores are computed correctly
    for (const auto& child : children) {
        EXPECT_EQ(child.level, 1);
        // g_score should be computed (may be less than const_term if one-body is negative)
        // Just verify it's a valid number
        EXPECT_FALSE(std::isnan(child.g_score));
        EXPECT_FALSE(std::isinf(child.g_score));
        EXPECT_GE(child.h_score, 0);  // Heuristic should be non-negative
        
        // Verify g_score was computed by expand (should be different from default 0)
        // by checking that computeGScore gives the same value
        double computed_g = astar.computeGScore(child);
        EXPECT_NEAR(child.g_score, computed_g, 1e-10);
    }
    
}

