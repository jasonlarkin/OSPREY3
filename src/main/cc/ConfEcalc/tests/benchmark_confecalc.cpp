/*
** Benchmark for ConfEcalc profiling
** 
** This creates a longer-running test to generate useful gprof data
*/

#include <iostream>
#include <chrono>

// Forward declare version functions (defined in confecalc.cc)
extern "C" {
    int version_major() noexcept;
    int version_minor() noexcept;
}

int main() {
    // Simple benchmark that calls version functions many times
    // In real profiling, you'd call actual energy calculation functions
    
    const int iterations = 1000000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < iterations; i++) {
        version_major();
        version_minor();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    std::cout << "Completed " << iterations << " iterations in " 
              << duration.count() << " ms" << std::endl;
    
    return 0;
}

