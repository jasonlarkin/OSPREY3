
#include <iostream>

// Demonstration: [[nodiscard]] prevents ignoring return values

// OLD: No attribute - can silently ignore return value
int old_calc_energy() {
    return 42;
}

// NEW: [[nodiscard]] - compiler warns if return value is ignored
[[nodiscard]] int new_calc_energy() {
    return 42;
}

int main() {
    std::cout << "=== [[nodiscard]] Demonstration ===\n\n";
    
    std::cout << "1. Both work when return value is used:\n";
    int e1 = old_calc_energy();
    int e2 = new_calc_energy();
    std::cout << "   old_calc_energy() = " << e1 << "\n";
    std::cout << "   new_calc_energy() = " << e2 << "\n\n";
    
    std::cout << "2. Ignoring return values:\n";
    std::cout << "   old_calc_energy(); - Compiles (bug not caught!)\n";
    std::cout << "   new_calc_energy(); - COMPILE WARNING (bug caught!)\n";
    std::cout << "   warning: ignoring return value of 'int new_calc_energy()'\n";
    std::cout << "   note: declared with attribute 'nodiscard'\n\n";
    
    // Uncomment to demonstrate warning:
    old_calc_energy();  // No warning
    new_calc_energy();  // Warning: ignoring return value
    
    std::cout << "3. Benefits:\n";
    std::cout << "   - Catches bugs where return values are accidentally ignored\n";
    std::cout << "   - Especially important for energy calculations\n";
    std::cout << "   - Zero runtime overhead\n";
    std::cout << "   - Compile-time safety\n\n";
    
    return 0;
}

