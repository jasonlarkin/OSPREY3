
#include <concepts>
#include <type_traits>
#include <iostream>
#include <cassert>

// Demonstration: Concepts provide better error messages
// This test shows how concepts catch errors at compile time with clearer messages

// OLD WAY (C++17): No compile-time constraint
template<typename T>
T old_style_calc(T x) {
    return x * 2.0;  // Accepts int but may not be intended
}

// NEW WAY (C++20): Concepts enforce type constraints
template<std::floating_point T>
T new_style_calc(T x) {
    return x * 2.0;  // Compiler guarantees T is floating point
}

// Demonstration: Concepts prevent incorrect usage
int main() {
    std::cout << "=== Concepts Demonstration ===\n\n";
    
    // Both work with floating point types
    std::cout << "1. Floating point types work with both:\n";
    std::cout << "   old_style_calc(3.14f) = " << old_style_calc(3.14f) << "\n";
    std::cout << "   new_style_calc(3.14f) = " << new_style_calc(3.14f) << "\n";
    std::cout << "   old_style_calc(2.71) = " << old_style_calc(2.71) << "\n";
    std::cout << "   new_style_calc(2.71) = " << new_style_calc(2.71) << "\n\n";
    
    // Old way allows integers (may be unintended)
    std::cout << "2. Integer types:\n";
    std::cout << "   old_style_calc(5) = " << old_style_calc(5) << " (works, but may be unintended)\n";
    // new_style_calc(5) fails to compile with clear error:
    // "no matching function for call to 'new_style_calc(int)'"
    // "note: candidate template ignored: constraints not satisfied"
    // "note: 'int' does not satisfy 'std::floating_point'"
    
    std::cout << "   new_style_calc(5) = COMPILE ERROR (prevents incorrect usage)\n\n";
    
    // Concepts provide compile-time type safety
    std::cout << "3. Benefits of concepts:\n";
    std::cout << "   - Compile-time type checking\n";
    std::cout << "   - Clearer error messages\n";
    std::cout << "   - Self-documenting code\n";
    std::cout << "   - Better IDE support\n";
    std::cout << "   - Zero runtime overhead\n\n";
    
    // Test that concepts work correctly
    static_assert(std::floating_point<float>);
    static_assert(std::floating_point<double>);
    static_assert(!std::floating_point<int>);
    
    std::cout << "All concept checks passed!\n";
    return 0;
}

