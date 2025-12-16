
#include <concepts>
#include <type_traits>
#include <iostream>
#include <cassert>
#include <string>
#include <cstdint>

// ============================================================================
// DEMONSTRATION: Concepts vs Old Template Approach
// ============================================================================

// OLD APPROACH (C++17): template<typename T> - accepts ANY type
template<typename T>
T old_energy_calc(T x, T y) {
    // Problem: What if someone passes int? Or std::string? Compiles but wrong!
    return x * y + 1.5;
}

// NEW APPROACH (C++20): template<std::floating_point T> - only floating point
template<std::floating_point T>
T new_energy_calc(T x, T y) {
    // Compiler enforces T must be float or double
    return x * y + 1.5;
}

// ============================================================================
// Test Case 1: Concepts prevent incorrect type usage
// ============================================================================

void test_type_safety() {
    std::cout << "=== Test 1: Type Safety ===\n";
    
    // Both work with floating point
    float f1 = 2.5f, f2 = 3.0f;
    double d1 = 2.5, d2 = 3.0;
    
    std::cout << "Floating point types (both work):\n";
    std::cout << "  old_energy_calc(2.5f, 3.0f) = " << old_energy_calc(f1, f2) << "\n";
    std::cout << "  new_energy_calc(2.5f, 3.0f) = " << new_energy_calc(f1, f2) << "\n";
    std::cout << "  old_energy_calc(2.5, 3.0) = " << old_energy_calc(d1, d2) << "\n";
    std::cout << "  new_energy_calc(2.5, 3.0) = " << new_energy_calc(d1, d2) << "\n\n";
    
    // OLD APPROACH: Compiles with integers (may be unintended!)
    int i1 = 2, i2 = 3;
    std::cout << "Integer types:\n";
    std::cout << "  old_energy_calc(2, 3) = " << old_energy_calc(i1, i2) << " (compiles, but wrong type!)\n";
    std::cout << "  new_energy_calc(2, 3) = COMPILE ERROR (concepts prevent this)\n\n";
    
    // OLD APPROACH: Even compiles with completely wrong types!
    // This would compile but crash at runtime:
    // std::string s1 = "hello", s2 = "world";
    // old_energy_calc(s1, s2);  // Compiles but makes no sense!
    std::cout << "Wrong types (string, etc.):\n";
    std::cout << "  old_energy_calc(\"hello\", \"world\") = Would compile but crash!\n";
    std::cout << "  new_energy_calc(\"hello\", \"world\") = COMPILE ERROR (caught early!)\n\n";
}

// ============================================================================
// Test Case 2: Better error messages
// ============================================================================

void test_error_messages() {
    std::cout << "=== Test 2: Error Message Quality ===\n";
    std::cout << "When calling with wrong type:\n\n";
    
    std::cout << "OLD APPROACH error (if it fails at all):\n";
    std::cout << "  error: no matching function for call\n";
    std::cout << "  note: candidate template ignored: couldn't infer template argument\n";
    std::cout << "  (Unclear what went wrong)\n\n";
    
    std::cout << "NEW APPROACH error (with concepts):\n";
    std::cout << "  error: no matching function for call to 'new_energy_calc(int, int)'\n";
    std::cout << "  note: candidate template ignored: constraints not satisfied\n";
    std::cout << "  note: 'int' does not satisfy 'std::floating_point'\n";
    std::cout << "  (Clear: int is not a floating point type)\n\n";
}

// ============================================================================
// Test Case 3: Template specialization issues
// ============================================================================

// OLD: Can accidentally specialize for wrong types
template<typename T>
T old_specialized(T x) {
    return x * 2;
}

// Specialization for int - but this shouldn't exist for energy calculations!
template<>
int old_specialized<int>(int x) {
    return x * 2;  // Compiles, but wrong type for energy calculations
}

// NEW: Concepts prevent specializing for wrong types
template<std::floating_point T>
T new_specialized(T x) {
    return x * 2;
}

// This would fail to compile:
// template<>
// int new_specialized<int>(int x) {  // ERROR: int doesn't satisfy std::floating_point
//     return x * 2;
// }

void test_specialization() {
    std::cout << "=== Test 3: Template Specialization ===\n";
    std::cout << "OLD: Can specialize for wrong types:\n";
    std::cout << "  old_specialized(5) = " << old_specialized(5) << " (int specialization exists)\n";
    std::cout << "  old_specialized(3.14) = " << old_specialized(3.14) << "\n\n";
    
    std::cout << "NEW: Concepts prevent wrong specializations:\n";
    std::cout << "  new_specialized(3.14) = " << new_specialized(3.14) << "\n";
    std::cout << "  new_specialized(5) = COMPILE ERROR (no int specialization possible)\n\n";
}

// ============================================================================
// Test Case 4: Real-world scenario - Array template
// ============================================================================

// Simulating Array<T> behavior
template<typename T>
class OldArray {
public:
    T* data;
    int64_t size;
    OldArray(int64_t s) : size(s), data(new T[s]) {}
    ~OldArray() { delete[] data; }
};

template<typename T>
requires std::is_trivially_copyable_v<T>
class NewArray {
public:
    T* data;
    int64_t size;
    NewArray(int64_t s) : size(s), data(new T[s]) {}
    ~NewArray() { delete[] data; }
};

void test_array_constraints() {
    std::cout << "=== Test 4: Array Type Constraints ===\n";
    
    // Both work with trivially copyable types
    std::cout << "Trivially copyable types (int, float, double):\n";
    OldArray<int> old_int_arr(10);
    NewArray<int> new_int_arr(10);
    std::cout << "  OldArray<int> - compiles\n";
    std::cout << "  NewArray<int> - compiles\n\n";
    
    OldArray<float> old_float_arr(10);
    NewArray<float> new_float_arr(10);
    std::cout << "  OldArray<float> - compiles\n";
    std::cout << "  NewArray<float> - compiles\n\n";
    
    // OLD: Compiles with non-trivially-copyable types (dangerous for JNA!)
    // This could cause issues with JNA memory layout
    struct NonTrivial {
        std::string s;  // Not trivially copyable
        NonTrivial() : s("test") {}
    };
    
    std::cout << "Non-trivially-copyable types:\n";
    std::cout << "  OldArray<NonTrivial> - compiles (but unsafe for JNA!)\n";
    std::cout << "  NewArray<NonTrivial> - COMPILE ERROR (concepts prevent unsafe usage)\n\n";
}

// ============================================================================
// Test Case 5: Compile-time validation
// ============================================================================

void test_compile_time_validation() {
    std::cout << "=== Test 5: Compile-Time Validation ===\n";
    
    // Concepts provide compile-time checks
    static_assert(std::floating_point<float>, "float is floating point");
    static_assert(std::floating_point<double>, "double is floating point");
    static_assert(!std::floating_point<int>, "int is NOT floating point");
    static_assert(!std::floating_point<std::string>, "string is NOT floating point");
    
    static_assert(std::is_trivially_copyable_v<int>, "int is trivially copyable");
    static_assert(std::is_trivially_copyable_v<float>, "float is trivially copyable");
    static_assert(!std::is_trivially_copyable_v<std::string>, "string is NOT trivially copyable");
    
    std::cout << "All compile-time assertions passed!\n";
    std::cout << "Concepts enable compile-time type validation.\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=================================================================\n";
    std::cout << "C++20 Concepts: Demonstrating Improvements Over Old Templates\n";
    std::cout << "=================================================================\n\n";
    
    test_type_safety();
    test_error_messages();
    test_specialization();
    test_array_constraints();
    test_compile_time_validation();
    
    std::cout << "=================================================================\n";
    std::cout << "Summary: Concepts provide:\n";
    std::cout << "  1. Compile-time type safety\n";
    std::cout << "  2. Better error messages\n";
    std::cout << "  3. Prevention of incorrect template specializations\n";
    std::cout << "  4. Self-documenting code\n";
    std::cout << "  5. Zero runtime overhead\n";
    std::cout << "=================================================================\n";
    
    return 0;
}

