#include <gtest/gtest.h>

#include <cstdint>
#include <initializer_list>
#include <string>

#include "conf_index.hpp"

using osprey::kstar::ConfIndex;

static void setPrefix(std::vector<int32_t>& dest, std::initializer_list<int32_t> src) {
    size_t i = 0;
    for (int32_t v : src) {
        dest.at(i++) = v;
    }
}

static void expectStartsWith(
    const std::vector<int32_t>& actual,
    int32_t actualCount,
    std::initializer_list<int32_t> expectedPrefix
) {
    ASSERT_GE(actualCount, static_cast<int32_t>(expectedPrefix.size()));
    size_t i = 0;
    for (int32_t expected : expectedPrefix) {
        EXPECT_EQ(actual.at(i), expected) << "prefix index " << i;
        i++;
    }
}

static ConfIndex makeRoot5() {
    ConfIndex confIndex(5);
    confIndex.numDefined = 0;
    confIndex.numUndefined = 5;
    setPrefix(confIndex.undefinedPos, {0, 1, 2, 3, 4});
    return confIndex;
}

// VERBATIM PORT of:
// src/test/java/edu/duke/cs/osprey/astar/TestConfIndex.java
// (JUnit5 + Hamcrest) -> (GoogleTest)

TEST(ConfIndex_VERBATIM, IsDefinedUndefinedRoot) {
    ConfIndex index = makeRoot5();
    index.numDefined = 0;
    index.numUndefined = 5;
    setPrefix(index.undefinedPos, {0, 1, 2, 3, 4});

    EXPECT_FALSE(index.isDefined(0));
    EXPECT_FALSE(index.isDefined(1));
    EXPECT_FALSE(index.isDefined(2));
    EXPECT_FALSE(index.isDefined(3));
    EXPECT_FALSE(index.isDefined(4));

    EXPECT_TRUE(index.isUndefined(0));
    EXPECT_TRUE(index.isUndefined(1));
    EXPECT_TRUE(index.isUndefined(2));
    EXPECT_TRUE(index.isUndefined(3));
    EXPECT_TRUE(index.isUndefined(4));
}

TEST(ConfIndex_VERBATIM, IsDefinedUndefinedChild) {
    ConfIndex index(5);
    index.numDefined = 3;
    setPrefix(index.definedPos, {0, 3, 4});
    setPrefix(index.definedRCs, {1, 2, 3});
    index.numUndefined = 2;
    setPrefix(index.undefinedPos, {1, 2});

    EXPECT_TRUE(index.isDefined(0));
    EXPECT_FALSE(index.isDefined(1));
    EXPECT_FALSE(index.isDefined(2));
    EXPECT_TRUE(index.isDefined(3));
    EXPECT_TRUE(index.isDefined(4));

    EXPECT_FALSE(index.isUndefined(0));
    EXPECT_TRUE(index.isUndefined(1));
    EXPECT_TRUE(index.isUndefined(2));
    EXPECT_FALSE(index.isUndefined(3));
    EXPECT_FALSE(index.isUndefined(4));
}

TEST(ConfIndex_VERBATIM, IsDefinedUndefinedWithGarbage) {
    ConfIndex index(5);
    index.numDefined = 3;
    setPrefix(index.definedPos, {0, 3, 4, 0, 1});
    setPrefix(index.definedRCs, {1, 2, 3, -1, -1});
    index.numUndefined = 2;
    setPrefix(index.undefinedPos, {1, 2, 4, 1, 3});

    EXPECT_TRUE(index.isDefined(0));
    EXPECT_FALSE(index.isDefined(1));
    EXPECT_FALSE(index.isDefined(2));
    EXPECT_TRUE(index.isDefined(3));
    EXPECT_TRUE(index.isDefined(4));

    EXPECT_FALSE(index.isUndefined(0));
    EXPECT_TRUE(index.isUndefined(1));
    EXPECT_TRUE(index.isUndefined(2));
    EXPECT_FALSE(index.isUndefined(3));
    EXPECT_FALSE(index.isUndefined(4));
}

TEST(ConfIndex_VERBATIM, AssignRoot) {
    // I AM ROOT
    ConfIndex index = makeRoot5().assign(3, 5);

    EXPECT_EQ(index.numPos, 5);
    EXPECT_EQ(index.numDefined, 1);
    expectStartsWith(index.definedPos, index.numDefined, {3});
    expectStartsWith(index.definedRCs, index.numDefined, {5});
}

TEST(ConfIndex_VERBATIM, AssignChildBefore) {
    ConfIndex index = makeRoot5().assign(3, 5).assign(1, 6);

    EXPECT_EQ(index.numPos, 5);
    EXPECT_EQ(index.numDefined, 2);
    expectStartsWith(index.definedPos, index.numDefined, {1, 3});
    expectStartsWith(index.definedRCs, index.numDefined, {6, 5});
}

TEST(ConfIndex_VERBATIM, AssignChildAfter) {
    ConfIndex index = makeRoot5().assign(3, 5).assign(4, 6);

    EXPECT_EQ(index.numPos, 5);
    EXPECT_EQ(index.numDefined, 2);
    expectStartsWith(index.definedPos, index.numDefined, {3, 4});
    expectStartsWith(index.definedRCs, index.numDefined, {5, 6});
}

TEST(ConfIndex_VERBATIM, AssignAll01234) {
    ConfIndex index = makeRoot5()
                          .assign(0, 4)
                          .assign(1, 5)
                          .assign(2, 6)
                          .assign(3, 7)
                          .assign(4, 8);

    EXPECT_EQ(index.numPos, 5);
    EXPECT_EQ(index.numDefined, 5);
    expectStartsWith(index.definedPos, index.numDefined, {0, 1, 2, 3, 4});
    expectStartsWith(index.definedRCs, index.numDefined, {4, 5, 6, 7, 8});
}

TEST(ConfIndex_VERBATIM, AssignAll43210) {
    ConfIndex index = makeRoot5()
                          .assign(4, 8)
                          .assign(3, 7)
                          .assign(2, 6)
                          .assign(1, 5)
                          .assign(0, 4);

    EXPECT_EQ(index.numPos, 5);
    EXPECT_EQ(index.numDefined, 5);
    expectStartsWith(index.definedPos, index.numDefined, {0, 1, 2, 3, 4});
    expectStartsWith(index.definedRCs, index.numDefined, {4, 5, 6, 7, 8});
}


