#include <gtest/gtest.h>

#include "astar_node.hpp"

using namespace osprey::kstar;

// VERBATIM port of OSPREY's TestLinkedConfAStarNode tests

TEST(AStarNode_VERBATIM, IndexRoot) {
    auto root = AStarNode<double>::root(5);

    EXPECT_EQ(root.level, 0);
    ASSERT_EQ(root.assignments.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        EXPECT_FALSE(root.isAssigned(i));
        EXPECT_EQ(root.getRC(i), -1);
    }
}

TEST(AStarNode_VERBATIM, IndexChild0) {
    auto root = AStarNode<double>::root(5);
    auto child = root.assign(0, 5);

    EXPECT_EQ(child.level, 1);
    EXPECT_TRUE(child.isAssigned(0));
    EXPECT_EQ(child.getRC(0), 5);
    for (int i = 1; i < 5; ++i) {
        EXPECT_FALSE(child.isAssigned(i));
    }
}

TEST(AStarNode_VERBATIM, IndexChild3) {
    auto root = AStarNode<double>::root(5);
    auto child = root.assign(3, 6);

    EXPECT_EQ(child.level, 1);
    EXPECT_TRUE(child.isAssigned(3));
    EXPECT_EQ(child.getRC(3), 6);
    for (int i = 0; i < 5; ++i) {
        if (i != 3) {
            EXPECT_FALSE(child.isAssigned(i));
        }
    }
}

TEST(AStarNode_VERBATIM, IndexChild03) {
    auto root = AStarNode<double>::root(5);
    auto child = root.assign(0, 5).assign(3, 6);

    EXPECT_EQ(child.level, 2);
    EXPECT_TRUE(child.isAssigned(0));
    EXPECT_EQ(child.getRC(0), 5);
    EXPECT_TRUE(child.isAssigned(3));
    EXPECT_EQ(child.getRC(3), 6);
    for (int i = 1; i < 3; ++i) {
        EXPECT_FALSE(child.isAssigned(i));
    }
    for (int i = 4; i < 5; ++i) {
        EXPECT_FALSE(child.isAssigned(i));
    }
}

TEST(AStarNode_VERBATIM, IndexChild30_OrderIndependence) {
    auto root = AStarNode<double>::root(5);
    auto child = root.assign(3, 6).assign(0, 5);

    EXPECT_EQ(child.level, 2);
    EXPECT_TRUE(child.isAssigned(0));
    EXPECT_EQ(child.getRC(0), 5);
    EXPECT_TRUE(child.isAssigned(3));
    EXPECT_EQ(child.getRC(3), 6);

    auto child2 = root.assign(0, 5).assign(3, 6);
    EXPECT_EQ(child.isAssigned(0), child2.isAssigned(0));
    EXPECT_EQ(child.getRC(0), child2.getRC(0));
    EXPECT_EQ(child.isAssigned(3), child2.isAssigned(3));
    EXPECT_EQ(child.getRC(3), child2.getRC(3));
}

TEST(AStarNode_VERBATIM, IndexChild01234_AllAssignedInOrder) {
    auto root = AStarNode<double>::root(5);
    auto child = root.assign(0, 4)
                     .assign(1, 5)
                     .assign(2, 6)
                     .assign(3, 7)
                     .assign(4, 8);

    EXPECT_EQ(child.level, 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(child.isAssigned(i));
    }
    EXPECT_EQ(child.getRC(0), 4);
    EXPECT_EQ(child.getRC(1), 5);
    EXPECT_EQ(child.getRC(2), 6);
    EXPECT_EQ(child.getRC(3), 7);
    EXPECT_EQ(child.getRC(4), 8);
}

TEST(AStarNode_VERBATIM, IndexChild43210_AllAssignedReverseOrder) {
    auto root = AStarNode<double>::root(5);
    auto child = root.assign(4, 8)
                     .assign(3, 7)
                     .assign(2, 6)
                     .assign(1, 5)
                     .assign(0, 4);

    EXPECT_EQ(child.level, 5);
    for (int i = 0; i < 5; ++i) {
        EXPECT_TRUE(child.isAssigned(i));
    }
    EXPECT_EQ(child.getRC(0), 4);
    EXPECT_EQ(child.getRC(1), 5);
    EXPECT_EQ(child.getRC(2), 6);
    EXPECT_EQ(child.getRC(3), 7);
    EXPECT_EQ(child.getRC(4), 8);
}

