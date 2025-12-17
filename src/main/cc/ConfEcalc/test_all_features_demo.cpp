


#include <concepts>
#include <type_traits>
#include <iostream>
#include <cassert>

// ============================================================================
// COMPREHENSIVE DEMONSTRATION: All C++20 Features Working Together
// ============================================================================
// This test demonstrates:
// 1. Concepts (std::floating_point) - compile-time type safety
// 2. [[nodiscard]] - prevent ignoring return values
// 3. noexcept - exception safety and optimization hints
// 4. Feature interactions - how they work together
// ============================================================================

// ============================================================================
// Feature 1: Concepts - Type Safety
// ============================================================================

// OLD: No type constraints
template<typename T>
T old_energy_calc(T x, T y) {
    return x * y + 1.5;
}

// NEW: Concepts enforce floating point types
template<std::floating_point T>
T new_energy_calc(T x, T y) {
    return x * y + 1.5;
}

// ============================================================================
// Feature 2: [[nodiscard]] - Return Value Safety
// ============================================================================

// OLD: Can silently ignore return value (bug!)
int old_get_energy() {
    return 42;
}

// NEW: Compiler warns if return value is ignored
[[nodiscard]] int new_get_energy() {
    return 42;
}

// ============================================================================
// Feature 3: noexcept - Exception Safety
// ============================================================================

// OLD: No exception guarantee
template<typename T>
T old_safe_get(T* ptr, int index) {
    return ptr[index];  // Could throw if ptr is null (undefined behavior)
}

// NEW: noexcept guarantees no exceptions (enables optimizations)
template<std::floating_point T>
[[nodiscard]] T new_safe_get(T* ptr, int index) noexcept {
    // In production code, bounds checking would be present.
    // Marked noexcept because ptr is guaranteed valid and index is in bounds.
    return ptr[index];
}

// ============================================================================
// Feature 4: ALL FEATURES TOGETHER - Real-World Pattern
// ============================================================================

// Simulating OSPREY energy calculation pattern with all C++20 features
template<std::floating_point T>
class EnergyCalculator {
public:
    // Constructor with concepts
    explicit EnergyCalculator(int size) noexcept : size_(size) {}
    
    // Getter with noexcept and [[nodiscard]]
    [[nodiscard]] int get_size() const noexcept {
        return size_;
    }
    
    // Energy calculation with all features
    template<std::floating_point U>
    [[nodiscard]] U calculate_energy(const U* coords, int count) const noexcept {
        U total = 0.0;
        for (int i = 0; i < count; ++i) {
            total += coords[i] * coords[i];  // Simplified energy calculation
        }
        return total;
    }
    
    // Distance calculation with all features
    template<std::floating_point U>
    [[nodiscard]] U distance_sq(const U* a, const U* b) const noexcept {
        U dx = a[0] - b[0];
        U dy = a[1] - b[1];
        U dz = a[2] - b[2];
        return dx*dx + dy*dy + dz*dz;
    }
    
private:
    int size_;
};

// ============================================================================
// Test 1: Concepts Prevent Wrong Types
// ============================================================================

void test_concepts() {
    std::cout << "=== Test 1: Concepts (Type Safety) ===\n";
    
    float f1 = 2.5f, f2 = 3.0f;
    double d1 = 2.5, d2 = 3.0;
    
    std::cout << "Floating point types work:\n";
    std::cout << "  new_energy_calc(2.5f, 3.0f) = " << new_energy_calc(f1, f2) << "\n";
    std::cout << "  new_energy_calc(2.5, 3.0) = " << new_energy_calc(d1, d2) << "\n";
    
    // Compilation fails for integer types:
    // int i1 = 2, i2 = 3;
    // new_energy_calc(i1, i2);  // ERROR: 'int' does not satisfy 'std::floating_point'
    
    std::cout << "  new_energy_calc(2, 3) = COMPILE ERROR (concepts prevent wrong types)\n\n";
}

// ============================================================================
// Test 2: [[nodiscard]] Prevents Ignoring Return Values
// ============================================================================

void test_nodiscard() {
    std::cout << "=== Test 2: [[nodiscard]] (Return Value Safety) ===\n";
    
    std::cout << "When return value is used:\n";
    int e1 = old_get_energy();
    int e2 = new_get_energy();
    std::cout << "  old_get_energy() = " << e1 << " (no warning)\n";
    std::cout << "  new_get_energy() = " << e2 << " (no warning)\n\n";
    
    std::cout << "When return value is ignored:\n";
    std::cout << "  old_get_energy(); - Compiles (bug not caught!)\n";
    std::cout << "  new_get_energy(); - COMPILE WARNING (bug caught!)\n";
    std::cout << "    warning: ignoring return value of 'int new_get_energy()'\n";
    std::cout << "    note: declared with attribute 'nodiscard'\n\n";
    
    // Uncomment to see the warning:
    // old_get_energy();  // No warning
    // new_get_energy();  // Warning!
}

// ============================================================================
// Test 3: noexcept Enables Optimizations
// ============================================================================

void test_noexcept() {
    std::cout << "=== Test 3: noexcept (Exception Safety & Optimization) ===\n";
    
    float arr[3] = {1.0f, 2.0f, 3.0f};
    
    std::cout << "noexcept functions:\n";
    float val = new_safe_get(arr, 1);
    std::cout << "  new_safe_get(arr, 1) = " << val << "\n";
    std::cout << "  Benefits:\n";
    std::cout << "    - Compiler can optimize more aggressively\n";
    std::cout << "    - Can be used in noexcept contexts\n";
    std::cout << "    - Documents exception safety guarantee\n";
    std::cout << "    - Enables move semantics in containers\n\n";
    
    // Demonstrate noexcept in template context
    static_assert(noexcept(new_safe_get(arr, 1)), "Function is noexcept");
    std::cout << "  Compile-time check: noexcept(new_safe_get) = true\n\n";
}

// ============================================================================
// Test 4: All Features Together
// ============================================================================

void test_all_features_together() {
    std::cout << "=== Test 4: All Features Working Together ===\n";
    
    // Create calculator with concepts
    EnergyCalculator<float> calc(10);
    
    // Use noexcept getter with [[nodiscard]]
    int size = calc.get_size();  // Warns if return value is ignored ([[nodiscard]])
    std::cout << "Calculator size: " << size << "\n";
    
    // Calculate energy with all features
    float coords[3] = {1.0f, 2.0f, 3.0f};
    float energy = calc.calculate_energy(coords, 3);  // Concepts + [[nodiscard]] + noexcept
    std::cout << "Energy: " << energy << "\n";
    
    // Distance calculation with all features
    float a[3] = {0.0f, 0.0f, 0.0f};
    float b[3] = {1.0f, 1.0f, 1.0f};
    float dist_sq = calc.distance_sq(a, b);  // Concepts + [[nodiscard]] + noexcept
    std::cout << "Distance squared: " << dist_sq << "\n\n";
    
    // Demonstrate that wrong types are caught by concepts
    std::cout << "Type safety (concepts):\n";
    std::cout << "  calc.calculate_energy(coords, 3) - OK (float)\n";
    // Compilation fails for integer types:
    // int int_coords[3] = {1, 2, 3};
    // calc.calculate_energy(int_coords, 3);  // ERROR: 'int' does not satisfy 'std::floating_point'
    std::cout << "  calc.calculate_energy(int_coords, 3) - COMPILE ERROR\n\n";
    
    // Demonstrate [[nodiscard]] protection
    std::cout << "Return value safety ([[nodiscard]]):\n";
    std::cout << "  energy = calc.calculate_energy(...) - OK (value used)\n";
    // Compilation warning when return value is ignored:
    // calc.calculate_energy(coords, 3);  // WARNING: ignoring return value
    std::cout << "  calc.calculate_energy(...); - COMPILE WARNING (value ignored)\n\n";
    
    // Demonstrate noexcept guarantee
    std::cout << "Exception safety (noexcept):\n";
    std::cout << "  calc.calculate_energy(...) - noexcept guaranteed\n";
    std::cout << "  Can be used in noexcept contexts\n";
    std::cout << "  Enables compiler optimizations\n\n";
}

// ============================================================================
// Test 5: Real-World OSPREY Pattern
// ============================================================================

void test_osprey_pattern() {
    std::cout << "=== Test 5: Real-World OSPREY Pattern ===\n";
    
    // Simulate OSPREY's energy calculation pattern
    EnergyCalculator<double> energy_calc(100);
    
    // Pattern: Multiple calculations in sequence
    double atom1[3] = {1.0, 2.0, 3.0};
    double atom2[3] = {4.0, 5.0, 6.0};
    double atom3[3] = {7.0, 8.0, 9.0};
    
    // All features working together:
    // 1. Concepts ensure double precision
    // 2. [[nodiscard]] requires return value usage
    // 3. noexcept enables optimizations
    double e1 = energy_calc.calculate_energy(atom1, 3);
    double e2 = energy_calc.calculate_energy(atom2, 3);
    double e3 = energy_calc.calculate_energy(atom3, 3);
    
    double dist12 = energy_calc.distance_sq(atom1, atom2);
    double dist23 = energy_calc.distance_sq(atom2, atom3);
    
    std::cout << "Energy calculations:\n";
    std::cout << "  Atom 1 energy: " << e1 << "\n";
    std::cout << "  Atom 2 energy: " << e2 << "\n";
    std::cout << "  Atom 3 energy: " << e3 << "\n";
    std::cout << "  Distance 1-2: " << dist12 << "\n";
    std::cout << "  Distance 2-3: " << dist23 << "\n\n";
    
    std::cout << "All features active:\n";
    std::cout << "  ✓ Concepts: Type safety (double enforced)\n";
    std::cout << "  ✓ [[nodiscard]]: Return values must be used\n";
    std::cout << "  ✓ noexcept: Exception safety guaranteed\n";
    std::cout << "  ✓ Zero runtime overhead: All compile-time\n\n";
}

// ============================================================================
// Test 6: Compile-Time Validation
// ============================================================================

void test_compile_time_validation() {
    std::cout << "=== Test 6: Compile-Time Validation ===\n";
    
    // Concepts provide compile-time checks
    static_assert(std::floating_point<float>, "float is floating point");
    static_assert(std::floating_point<double>, "double is floating point");
    static_assert(!std::floating_point<int>, "int is NOT floating point");
    
    // noexcept can be checked at compile time
    float arr[3] = {1.0f, 2.0f, 3.0f};
    static_assert(noexcept(new_safe_get(arr, 1)), "new_safe_get is noexcept");
    
    // [[nodiscard]] is checked at compile time (warnings)
    // Cannot be tested with static_assert; compiler generates warnings
    
    std::cout << "All compile-time assertions passed!\n";
    std::cout << "Concepts, noexcept, and [[nodiscard]] all provide compile-time safety.\n\n";
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "=================================================================\n";
    std::cout << "C++20 Features: Comprehensive Demonstration\n";
    std::cout << "=================================================================\n\n";
    
    test_concepts();
    test_nodiscard();
    test_noexcept();
    test_all_features_together();
    test_osprey_pattern();
    test_compile_time_validation();
    
    std::cout << "=================================================================\n";
    std::cout << "Summary: All C++20 Features Provide:\n";
    std::cout << "  1. Concepts: Compile-time type safety\n";
    std::cout << "  2. [[nodiscard]]: Return value safety\n";
    std::cout << "  3. noexcept: Exception safety & optimization hints\n";
    std::cout << "  4. Together: Comprehensive compile-time safety\n";
    std::cout << "  5. Zero runtime overhead: All compile-time features\n";
    std::cout << "=================================================================\n";
    
    return 0;
}

