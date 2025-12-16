
#include <gtest/gtest.h>
#include <type_traits>
#include <string>
#include "global.h"
#include "array.h"

using namespace osprey;

// Test Array: Trivially Copyable Constraint
TEST(ArrayTest, TriviallyCopyableAcceptsInt) {
    Array<int> arr(10);
    EXPECT_EQ(arr.get_size(), 10);
    arr[0] = 42;
    EXPECT_EQ(arr[0], 42);
    
    static_assert(std::is_trivially_copyable_v<int>);
}

TEST(ArrayTest, TriviallyCopyableAcceptsFloat) {
    Array<float> arr(10);
    EXPECT_EQ(arr.get_size(), 10);
    arr[0] = 3.14f;
    EXPECT_FLOAT_EQ(arr[0], 3.14f);
    
    static_assert(std::is_trivially_copyable_v<float>);
}

TEST(ArrayTest, TriviallyCopyableAcceptsDouble) {
    Array<double> arr(10);
    EXPECT_EQ(arr.get_size(), 10);
    arr[0] = 2.71;
    EXPECT_DOUBLE_EQ(arr[0], 2.71);
    
    static_assert(std::is_trivially_copyable_v<double>);
}

TEST(ArrayTest, TriviallyCopyableRejectsString) {
    // Verify string is NOT trivially copyable
    static_assert(!std::is_trivially_copyable_v<std::string>);
    
    // This would fail to compile:
    // Array<std::string> arr(10);  // ERROR: 'std::string' does not satisfy constraint
}

// Test Array: noexcept specifications
TEST(ArrayTest, GetSizeIsNoexcept) {
    Array<int> arr(10);
    static_assert(noexcept(arr.get_size()));
    
    int64_t size = arr.get_size();
    EXPECT_EQ(size, 10);
}

TEST(ArrayTest, OperatorBracketIsNoexcept) {
    Array<int> arr(10);
    arr[0] = 42;
    
    static_assert(noexcept(arr[0]));
    static_assert(noexcept(arr.get_size()));
    
    EXPECT_EQ(arr[0], 42);
}

// Test Array: Basic functionality with concepts
TEST(ArrayTest, BasicOperations) {
    Array<float> arr(5);
    
    EXPECT_EQ(arr.get_size(), 5);
    
    arr[0] = 1.0f;
    arr[1] = 2.0f;
    arr[2] = 3.0f;
    
    EXPECT_FLOAT_EQ(arr[0], 1.0f);
    EXPECT_FLOAT_EQ(arr[1], 2.0f);
    EXPECT_FLOAT_EQ(arr[2], 3.0f);
}

// Test Array: Const access
TEST(ArrayTest, ConstAccess) {
    Array<int> arr(3);
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    
    const Array<int>& const_arr = arr;
    EXPECT_EQ(const_arr[0], 10);
    EXPECT_EQ(const_arr[1], 20);
    EXPECT_EQ(const_arr[2], 30);
    
    static_assert(noexcept(const_arr[0]));
}

