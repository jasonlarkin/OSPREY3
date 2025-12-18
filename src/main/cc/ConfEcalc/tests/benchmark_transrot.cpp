#include "global.h"
#include "array.h"
#include "formats.h"
#include "real3.h"
#include "rotation.h"
#include "atoms.h"
#include "confspace.h"
#include "assignment.h"
#include "energy.h"
#include "motions.h"
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <cmath>
#include <numeric>
#include <random>

using namespace osprey;
using namespace osprey::motions;

// Simplified test for TranslationRotation apply() performance
// We can't easily create a full TranslationRotation without a ConfSpace,
// so we'll benchmark the core transform operations

void benchmark_transform_operations(int num_atoms, int iterations) {
    std::cout << "\n=== Benchmark: Transform Operations (TranslationRotation pattern) ===" << std::endl;
    std::cout << "Atoms: " << num_atoms << ", Iterations: " << iterations << std::endl;
    
    // Create rotation matrices
    Rotation<double> rot_current, rot_next;
    rot_current.set_xyz(0.3, 0.7, -0.4);
    rot_next.set_xyz(0.5, 1.2, -0.8);
    
    Real3<double> trans_current(1.0, 2.0, 3.0);
    Real3<double> trans_next(4.0, 5.0, 6.0);
    Real3<double> centroid(10.0, 20.0, 30.0);
    
    // Create atom arrays
    std::vector<Real3<double>> atoms_scalar(num_atoms);
    std::vector<Real3<double>> atoms_simd(num_atoms);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-10.0, 10.0);
    for (int i = 0; i < num_atoms; i++) {
        atoms_scalar[i] = Real3<double>(dist(rng), dist(rng), dist(rng));
        atoms_simd[i] = atoms_scalar[i];
    }
    
    // Scalar benchmark (mimics TranslationRotation::apply() loop)
    auto start = std::chrono::high_resolution_clock::now();
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < num_atoms; i++) {
            Real3<double>& p = atoms_scalar[i];
            p -= centroid;
            p = rot_current * (p + trans_current);
            p = rot_next * p + trans_next;
            p += centroid;
        }
        // Restore for next iteration
        for (int i = 0; i < num_atoms; i++) {
            atoms_scalar[i] = Real3<double>(dist(rng), dist(rng), dist(rng));
        }
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    
    // SIMD benchmark would go here if we had a vectorized version
    // For now, just report scalar performance
    
    std::cout << "Scalar time: " << scalar_time << " us" << std::endl;
    std::cout << "Time per atom: " << std::fixed << std::setprecision(3) 
              << (double)scalar_time / (num_atoms * iterations) << " us" << std::endl;
    
#ifdef USE_SIMD
    std::cout << "Note: SIMD version would use rotate_vectors_avx2 and transform_vectors_avx2" << std::endl;
#else
    std::cout << "SIMD not enabled" << std::endl;
#endif
}

int main(int argc, char* argv[]) {
    // Parse arguments: num_atoms iterations
    // Typical molecule sizes from workload analysis:
    // - Small molecule: 50-100 atoms
    // - Medium molecule: 100-300 atoms  
    // - Large molecule: 300-500 atoms
    int num_atoms = argc > 1 ? std::atoi(argv[1]) : 100;
    int iterations = argc > 2 ? std::atoi(argv[2]) : 10000;
    
    std::cout << "=== TranslationRotation Transform Benchmark ===" << std::endl;
    std::cout << "Number of atoms: " << num_atoms << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    
    benchmark_transform_operations(num_atoms, iterations);
    
    return 0;
}

