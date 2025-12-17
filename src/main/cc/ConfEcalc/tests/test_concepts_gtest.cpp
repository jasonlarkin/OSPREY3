
#include <gtest/gtest.h>
#include <concepts>
#include <type_traits>

// Test template using concepts
template<std::floating_point T>
T test_float_calc(T x) {
    return x * 2.0;
}

template<typename T>
requires std::is_trivially_copyable_v<T>
void test_array_func(T x) {
    (void)x;
}

// SFINAE helper to check if a type satisfies a concept
template<typename T>
concept satisfies_floating_point = std::floating_point<T>;

template<typename T>
concept satisfies_trivially_copyable = std::is_trivially_copyable_v<T>;

// Test Concepts: Floating Point Constraint
TEST(ConceptsTest, FloatingPointAcceptsFloat) {
    float f = 3.14f;
    float result = test_float_calc(f);
    EXPECT_FLOAT_EQ(result, 6.28f);
    
    // Verify concept is satisfied
    static_assert(satisfies_floating_point<float>);
    static_assert(std::floating_point<float>);
}

TEST(ConceptsTest, FloatingPointAcceptsDouble) {
    double d = 2.71;
    double result = test_float_calc(d);
    EXPECT_DOUBLE_EQ(result, 5.42);
    
    // Verify concept is satisfied
    static_assert(satisfies_floating_point<double>);
    static_assert(std::floating_point<double>);
}

TEST(ConceptsTest, FloatingPointRejectsInt) {
    // Verify int does NOT satisfy floating_point concept
    static_assert(!satisfies_floating_point<int>);
    static_assert(!std::floating_point<int>);
    
    // This would fail to compile:
    // int i = 5;
    // test_float_calc(i);  // ERROR: 'int' does not satisfy 'std::floating_point'
}

TEST(ConceptsTest, FloatingPointRejectsString) {
    // Verify string does NOT satisfy floating_point concept
    static_assert(!satisfies_floating_point<std::string>);
    static_assert(!std::floating_point<std::string>);
}

// Test Concepts: Trivially Copyable Constraint
TEST(ConceptsTest, TriviallyCopyableAcceptsInt) {
    int i = 42;
    test_array_func(i);
    
    static_assert(satisfies_trivially_copyable<int>);
    static_assert(std::is_trivially_copyable_v<int>);
}

TEST(ConceptsTest, TriviallyCopyableAcceptsFloat) {
    float f = 3.14f;
    test_array_func(f);
    
    static_assert(satisfies_trivially_copyable<float>);
    static_assert(std::is_trivially_copyable_v<float>);
}

TEST(ConceptsTest, TriviallyCopyableRejectsString) {
    // Verify string does NOT satisfy trivially_copyable
    static_assert(!satisfies_trivially_copyable<std::string>);
    static_assert(!std::is_trivially_copyable_v<std::string>);
    
    // This would fail to compile:
    // std::string s = "test";
    // test_array_func(s);  // ERROR: 'std::string' does not satisfy constraint
}

// Test Concepts: Compile-Time Validation
TEST(ConceptsTest, CompileTimeValidation) {
    // All these should compile and pass
    static_assert(std::floating_point<float>);
    static_assert(std::floating_point<double>);
    static_assert(!std::floating_point<int>);
    static_assert(!std::floating_point<std::string>);
    
    static_assert(std::is_trivially_copyable_v<int>);
    static_assert(std::is_trivially_copyable_v<float>);
    static_assert(std::is_trivially_copyable_v<double>);
    static_assert(!std::is_trivially_copyable_v<std::string>);
    
    SUCCEED();  // Test passes if all static_asserts compile
}

// Test Concepts: Template Instantiation
TEST(ConceptsTest, TemplateInstantiation) {
    // Verify templates can be instantiated with correct types
    EXPECT_NO_THROW({
        float f = test_float_calc(1.0f);
        double d = test_float_calc(1.0);
        (void)f;
        (void)d;
    });
    
    EXPECT_NO_THROW({
        test_array_func(42);
        test_array_func(3.14f);
        test_array_func(2.71);
    });
}

// Test Concepts: Error Message Quality
// This test documents what error messages concepts provide
TEST(ConceptsTest, ErrorMessageDocumentation) {
    // When concepts reject a type, the error message is:
    // "error: no matching function for call to 'test_float_calc(int)'"
    // "note: candidate template ignored: constraints not satisfied"
    // "note: 'int' does not satisfy 'std::floating_point'"
    
    // This is better than old template errors which were unclear
    SUCCEED();
}

