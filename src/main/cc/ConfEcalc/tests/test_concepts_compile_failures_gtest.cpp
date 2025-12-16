
#include <gtest/gtest.h>
#include <concepts>
#include <type_traits>
#include <string>

// Template with concepts that should reject certain types
template<std::floating_point T>
T test_calc(T x) {
    return x * 2.0;
}

// SFINAE helper to check if a type would be rejected
template<typename T>
concept satisfies_floating_point = std::floating_point<T>;

// Test that concepts correctly reject wrong types at compile time
TEST(ConceptsCompileFailures, IntRejected) {
    // Verify int does NOT satisfy floating_point
    static_assert(!satisfies_floating_point<int>);
    static_assert(!std::floating_point<int>);
    
    // This would fail to compile:
    // int i = 5;
    // test_calc(i);  // ERROR: 'int' does not satisfy 'std::floating_point'
    
    SUCCEED();
}

TEST(ConceptsCompileFailures, StringRejected) {
    // Verify string does NOT satisfy floating_point
    static_assert(!satisfies_floating_point<std::string>);
    static_assert(!std::floating_point<std::string>);
    
    // This would fail to compile:
    // std::string s = "test";
    // test_calc(s);  // ERROR: 'std::string' does not satisfy 'std::floating_point'
    
    SUCCEED();
}

TEST(ConceptsCompileFailures, CharRejected) {
    static_assert(!std::floating_point<char>);
    static_assert(!std::floating_point<signed char>);
    static_assert(!std::floating_point<unsigned char>);
    
    SUCCEED();
}

TEST(ConceptsCompileFailures, BoolRejected) {
    static_assert(!std::floating_point<bool>);
    
    SUCCEED();
}

TEST(ConceptsCompileFailures, PointerRejected) {
    static_assert(!std::floating_point<int*>);
    static_assert(!std::floating_point<float*>);
    
    SUCCEED();
}

// Test that correct types are accepted
TEST(ConceptsCompileFailures, FloatAccepted) {
    static_assert(std::floating_point<float>);
    float f = 3.14f;
    float result = test_calc(f);
    EXPECT_FLOAT_EQ(result, 6.28f);
}

TEST(ConceptsCompileFailures, DoubleAccepted) {
    static_assert(std::floating_point<double>);
    double d = 2.71;
    double result = test_calc(d);
    EXPECT_DOUBLE_EQ(result, 5.42);
}

// Test error message quality documentation
TEST(ConceptsCompileFailures, ErrorMessageDocumentation) {
    // When concepts reject a type, the error message is:
    // "error: no matching function for call to 'test_calc(int)'"
    // "note: candidate template ignored: constraints not satisfied"
    // "note: 'int' does not satisfy 'std::floating_point'"
    // This is clearer than old template errors.
    
    SUCCEED();
}

