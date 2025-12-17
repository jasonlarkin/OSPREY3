
#include <gtest/gtest.h>

// OLD: No attribute - can silently ignore return value
int old_calc_energy() {
    return 42;
}

// NEW: [[nodiscard]] - compiler warns if return value is ignored
[[nodiscard]] int new_calc_energy() {
    return 42;
}

// Test [[nodiscard]]: Return Value Used
TEST(NoDiscardTest, ReturnValueUsed) {
    int e1 = old_calc_energy();
    int e2 = new_calc_energy();
    EXPECT_EQ(e1, 42);
    EXPECT_EQ(e2, 42);
}

// Test [[nodiscard]]: Compile-Time Check
// Note: We can't test the warning directly in Google Test, but we can verify
// the function exists and works correctly when the return value is used.
TEST(NoDiscardTest, FunctionExists) {
    // This test verifies the function compiles and works
    int result = new_calc_energy();
    EXPECT_EQ(result, 42);
    
    // If we uncommented the line below, it would generate a warning:
    // new_calc_energy();  // WARNING: ignoring return value
}

// Test [[nodiscard]]: Multiple Return Types
[[nodiscard]] float calc_energy_float() {
    return 3.14f;
}

[[nodiscard]] double calc_energy_double() {
    return 2.71;
}

TEST(NoDiscardTest, MultipleTypes) {
    float f = calc_energy_float();
    double d = calc_energy_double();
    EXPECT_FLOAT_EQ(f, 3.14f);
    EXPECT_DOUBLE_EQ(d, 2.71);
}

