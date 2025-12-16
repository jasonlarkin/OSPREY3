
#include <concepts>
#include <type_traits>
#include <iostream>
#include <cassert>

// Test 3: Template constraints compile
// Verifies template syntax is correct
template<std::floating_point T>
void test_float_func(T x) {
    (void)x; // suppress unused warning
}

template<typename T>
requires std::is_trivially_copyable_v<T>
void test_array_func(T x) {
    (void)x; // suppress unused warning
}

// Test that concepts compile correctly
int main() {
    // Test 1: floating_point concept
    static_assert(std::floating_point<float>);
    static_assert(std::floating_point<double>);
    static_assert(!std::floating_point<int>);
    
    // Test 2: trivially_copyable for Array
    static_assert(std::is_trivially_copyable_v<int>);
    static_assert(std::is_trivially_copyable_v<float>);
    static_assert(std::is_trivially_copyable_v<double>);
    
    // Instantiate templates to verify they compile
    test_float_func(1.0f);
    test_float_func(1.0);
    test_array_func(42);
    test_array_func(3.14f);
    
    std::cout << "All concept checks passed!" << std::endl;
    std::cout << "Template constraints compile successfully!" << std::endl;
    
    return 0;
}

