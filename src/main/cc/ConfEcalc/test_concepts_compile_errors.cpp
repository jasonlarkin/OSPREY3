
// This file demonstrates compile-time errors that concepts catch.
// NOTE: This file is INTENTIONALLY designed to fail compilation.
// It demonstrates the improved error messages that concepts provide.
// To see the errors, compile this file:
//   g++ -std=c++20 test_concepts_compile_errors.cpp
//
// Expected errors:
// - "no matching function for call to 'new_calc(int)'"
// - "candidate template ignored: constraints not satisfied"
// - "'int' does not satisfy 'std::floating_point'"

#include <concepts>
#include <type_traits>
#include <string>

// OLD: No constraints - accepts any type
template<typename T>
T old_calc(T x) {
    return x * 2.0;
}

// NEW: Concepts enforce floating point - rejects wrong types
template<std::floating_point T>
T new_calc(T x) {
    return x * 2.0;
}

int main() {
    // These work with both:
    float f = 3.14f;
    double d = 2.71;
    old_calc(f);
    new_calc(f);
    old_calc(d);
    new_calc(d);
    
    // ERROR CASE 1: Integer type
    // Concepts reject this at compile time with clear error message
    int i = 5;
    new_calc(i);  // ERROR: 'int' does not satisfy 'std::floating_point'
    old_calc(i);  // Compiles (but may be unintended)
    
    // ERROR CASE 2: Wrong type entirely
    // Concepts reject this at compile time, preventing runtime crashes
    std::string s = "test";
    new_calc(s);  // ERROR: 'std::string' does not satisfy 'std::floating_point'
    // old_calc(s);  // Would compile but crash at runtime
    
    return 0;
}

