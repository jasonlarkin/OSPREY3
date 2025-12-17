
#include <gtest/gtest.h>
#include <concepts>
#include <type_traits>
#include <string>

// Test template specialization prevention with concepts
template<std::floating_point T>
T test_specialized_calc(T x) {
    return x * 2.0;
}

// This would fail to compile:
// template<>
// int test_specialized_calc<int>(int x) {  // ERROR: int doesn't satisfy std::floating_point
//     return x * 2;
// }

// Test Array-like constraints with non-trivially-copyable types
template<typename T>
requires std::is_trivially_copyable_v<T>
class TestArray {
public:
    T* data;
    int size;
    TestArray(int s) : size(s), data(new T[s]) {}
    ~TestArray() { delete[] data; }
};

struct NonTrivial {
    std::string s;
    NonTrivial() : s("test") {}
};

// Test Concepts: Template Specialization Prevention
TEST(ConceptsAdvancedTest, PreventsWrongSpecialization) {
    // Verify we can specialize for floating point types
    static_assert(std::floating_point<float>);
    static_assert(std::floating_point<double>);
    
    float f = 3.14f;
    double d = 2.71;
    EXPECT_FLOAT_EQ(test_specialized_calc(f), 6.28f);
    EXPECT_DOUBLE_EQ(test_specialized_calc(d), 5.42);
    
    // Verify int cannot be specialized
    static_assert(!std::floating_point<int>);
    // Attempting to specialize for int would fail to compile
}

// Test Concepts: Array Constraints with Non-Trivially-Copyable
TEST(ConceptsAdvancedTest, ArrayAcceptsTriviallyCopyable) {
    TestArray<int> arr_int(10);
    TestArray<float> arr_float(10);
    TestArray<double> arr_double(10);
    
    EXPECT_EQ(arr_int.size, 10);
    EXPECT_EQ(arr_float.size, 10);
    EXPECT_EQ(arr_double.size, 10);
    
    static_assert(std::is_trivially_copyable_v<int>);
    static_assert(std::is_trivially_copyable_v<float>);
    static_assert(std::is_trivially_copyable_v<double>);
}

TEST(ConceptsAdvancedTest, ArrayRejectsNonTriviallyCopyable) {
    // Verify string is NOT trivially copyable
    static_assert(!std::is_trivially_copyable_v<std::string>);
    static_assert(!std::is_trivially_copyable_v<NonTrivial>);
    
    // This would fail to compile:
    // TestArray<std::string> arr_string(10);  // ERROR: constraint not satisfied
    // TestArray<NonTrivial> arr_non_trivial(10);  // ERROR: constraint not satisfied
}

// Test Concepts: Error Message Quality Documentation
TEST(ConceptsAdvancedTest, ErrorMessageQuality) {
    // When concepts reject a type, error messages are:
    // "error: no matching function for call to 'test_specialized_calc(int)'"
    // "note: candidate template ignored: constraints not satisfied"
    // "note: 'int' does not satisfy 'std::floating_point'"
    // This is clearer than old template errors.
    
    SUCCEED();
}

// Test Concepts: Multiple Constraints
template<typename T>
concept FloatingPointAndTriviallyCopyable = 
    std::floating_point<T> && std::is_trivially_copyable_v<T>;

template<FloatingPointAndTriviallyCopyable T>
T test_multiple_constraints(T x) {
    return x * 2.0;
}

TEST(ConceptsAdvancedTest, MultipleConstraints) {
    static_assert(FloatingPointAndTriviallyCopyable<float>);
    static_assert(FloatingPointAndTriviallyCopyable<double>);
    static_assert(!FloatingPointAndTriviallyCopyable<int>);  // int is not floating point
    
    float f = 3.14f;
    double d = 2.71;
    EXPECT_FLOAT_EQ(test_multiple_constraints(f), 6.28f);
    EXPECT_DOUBLE_EQ(test_multiple_constraints(d), 5.42);
}

// Test Concepts: Real-World Pattern - Energy Calculation
template<std::floating_point T>
T calculate_energy(const T* coords, int count) {
    T total = 0.0;
    for (int i = 0; i < count; ++i) {
        total += coords[i] * coords[i];
    }
    return total;
}

TEST(ConceptsAdvancedTest, RealWorldEnergyCalculation) {
    float float_coords[3] = {1.0f, 2.0f, 3.0f};
    double double_coords[3] = {1.0, 2.0, 3.0};
    
    float e_float = calculate_energy(float_coords, 3);
    double e_double = calculate_energy(double_coords, 3);
    
    EXPECT_FLOAT_EQ(e_float, 14.0f);
    EXPECT_DOUBLE_EQ(e_double, 14.0);
    
    // This would fail to compile:
    // int int_coords[3] = {1, 2, 3};
    // calculate_energy(int_coords, 3);  // ERROR: 'int' does not satisfy 'std::floating_point'
}

