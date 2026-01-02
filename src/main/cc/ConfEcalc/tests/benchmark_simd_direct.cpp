/*
 * Direct C++ benchmark for SIMD energy calculations
 * 
 * This benchmark directly tests calc_scalar, calc_avx2, and calc_avx512
 * with synthetic test data to verify correctness and measure performance.
 */

// Include standard headers FIRST before any namespace pollution
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <vector>
#include <cstring>
#include <cstdint>

// Include OSPREY headers in the same order as confecalc.cc to satisfy dependencies
#include "../global.h"
#include "../array.h"
#include "../formats.h"
#include "../real3.h"
#include "../atoms.h"
#include "../confspace.h"
#include "../assignment.h"
#include "../energy.h"
#include "../energy_ambereef1.h"

using namespace osprey;

// Create test data
void create_test_data(
    Array<Real3<double>>& atoms,
    ambereef1::Params& params,
    std::vector<uint8_t>& pairs_buffer,
    int num_atoms = 100,
    int num_amber_pairs = 1000,
    int num_eef1_pairs = 500
) {
    // atoms should already be initialized with correct size
    // Use a seed for reproducibility and ensure atoms are well-separated
    srand(42);  // Seed for reproducibility
    for (int i = 0; i < num_atoms; i++) {
        // Spread atoms in a 3D grid to avoid coincident positions
        atoms[i] = Real3<double>(
            0.1 + 1.0 * (double)(i % 10),
            0.1 + 1.0 * (double)((i / 10) % 10),
            0.1 + 1.0 * (double)(i / 100)
        );
    }
    
    // Set params
    params.distance_dependent_dielectric = false;
    
    // Calculate buffer size needed
    size_t pairs_header_size = sizeof(ambereef1::AtomPairs);
    size_t amber_pairs_size = num_amber_pairs * sizeof(ambereef1::AtomPairAmber<double>);
    size_t eef1_pairs_size = num_eef1_pairs * sizeof(ambereef1::AtomPairEef1<double>);
    size_t total_size = pairs_header_size + amber_pairs_size + eef1_pairs_size;
    
    pairs_buffer.resize(total_size);
    memset(pairs_buffer.data(), 0, total_size);
    
    // Set header
    ambereef1::AtomPairs* pairs_header = reinterpret_cast<ambereef1::AtomPairs*>(pairs_buffer.data());
    pairs_header->num_amber = num_amber_pairs;
    pairs_header->num_eef1 = num_eef1_pairs;
    
    // Create amber pairs - ensure atomi1 != atomi2 to avoid r=0 (division by zero)
    ambereef1::AtomPairAmber<double>* amber_pairs = 
        reinterpret_cast<ambereef1::AtomPairAmber<double>*>(pairs_buffer.data() + pairs_header_size);
    for (int i = 0; i < num_amber_pairs; i++) {
        int a1 = rand() % num_atoms;
        int a2;
        do {
            a2 = rand() % num_atoms;
        } while (a2 == a1);  // Ensure different atoms
        amber_pairs[i].atomi1 = a1;
        amber_pairs[i].atomi2 = a2;
        amber_pairs[i].esQ = 1.0 + 10.0 * (double)rand() / RAND_MAX;  // Positive values
        amber_pairs[i].vdwA = 1000.0 + 1000.0 * (double)rand() / RAND_MAX;
        amber_pairs[i].vdwB = 10.0 + 10.0 * (double)rand() / RAND_MAX;
    }
    
    // Create eef1 pairs - ensure atomi1 != atomi2
    ambereef1::AtomPairEef1<double>* eef1_pairs = 
        reinterpret_cast<ambereef1::AtomPairEef1<double>*>(
            pairs_buffer.data() + pairs_header_size + amber_pairs_size);
    for (int i = 0; i < num_eef1_pairs; i++) {
        int a1 = rand() % num_atoms;
        int a2;
        do {
            a2 = rand() % num_atoms;
        } while (a2 == a1);  // Ensure different atoms
        eef1_pairs[i].atomi1 = a1;
        eef1_pairs[i].atomi2 = a2;
        eef1_pairs[i].vdwRadius1 = 1.5 + 0.5 * (double)rand() / RAND_MAX;
        eef1_pairs[i].lambda1 = 0.5 + 0.2 * (double)rand() / RAND_MAX;
        eef1_pairs[i].vdwRadius2 = 1.5 + 0.5 * (double)rand() / RAND_MAX;
        eef1_pairs[i].lambda2 = 0.5 + 0.2 * (double)rand() / RAND_MAX;
        eef1_pairs[i].alpha1 = 1.0 + (double)rand() / RAND_MAX;
        eef1_pairs[i].alpha2 = 1.0 + (double)rand() / RAND_MAX;
    }
}

// Benchmark a single function
double benchmark_function(
    double (*calc_func)(const Array<Real3<double>>&, const ambereef1::Params&, const ambereef1::AtomPairs&),
    const Array<Real3<double>>& atoms,
    const ambereef1::Params& params,
    const ambereef1::AtomPairs& pairs,
    int iterations,
    const char* name
) {
    std::cout << "  Benchmarking " << name << "... ";
    std::cout.flush();
    
    // Warmup
    for (int i = 0; i < 10; i++) {
        calc_func(atoms, params, pairs);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    volatile double total_energy = 0.0;  // volatile to prevent optimization
    for (int i = 0; i < iterations; i++) {
        total_energy = total_energy + calc_func(atoms, params, pairs);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double avg_us = (double)duration.count() / iterations;
    
    std::cout << std::fixed << std::setprecision(2) 
              << avg_us << " us/iter (total: " << total_energy << ")" << std::endl;
    
    return avg_us;
}

int main(int argc, char* argv[]) {
    int iterations = 1000;
    if (argc > 1) {
        iterations = std::atoi(argv[1]);
    }
    
    std::cout << "=== Direct C++ SIMD Benchmark ===" << std::endl;
    std::cout << "Iterations per test: " << iterations << std::endl;
    std::cout << std::endl;
    
    // Create test data
    Array<Real3<double>> atoms(200);  // Initialize with size
    ambereef1::Params params;
    std::vector<uint8_t> pairs_buffer;
    
    std::cout << "Creating test data..." << std::endl;
    create_test_data(atoms, params, pairs_buffer, 200, 2000, 1000);
    const ambereef1::AtomPairs& pairs = *reinterpret_cast<const ambereef1::AtomPairs*>(pairs_buffer.data());
    
    std::cout << "  Atoms: " << atoms.get_size() << std::endl;
    std::cout << "  Amber pairs: " << pairs.num_amber << std::endl;
    std::cout << "  EEF1 pairs: " << pairs.num_eef1 << std::endl;
    std::cout << std::endl;
    
    // Test correctness first
    std::cout << "=== Correctness Test ===" << std::endl;
    
#ifdef USE_SIMD
    // Force scalar mode (by directly calling calc_scalar)
    double energy_scalar = ambereef1::calc_scalar(atoms, params, pairs);
    
    std::cout << "Scalar energy:   " << std::fixed << std::setprecision(10) << energy_scalar << std::endl;
    
    // Test all versions
    // AVX2 (exact exp)
    double energy_avx2 = ambereef1::calc_avx2(atoms, params, pairs);
    std::cout << "AVX2 energy:     " << std::fixed << std::setprecision(10) << energy_avx2 << std::endl;
    double diff_avx2 = std::abs(energy_scalar - energy_avx2);
    double rel_diff_avx2 = diff_avx2 / std::abs(energy_scalar);
    std::cout << "  Difference:    " << diff_avx2 << " (relative: " 
              << std::scientific << rel_diff_avx2 << ")" << std::endl;
    if (rel_diff_avx2 < 1e-10) {
        std::cout << "  AVX2 matches scalar (correctness verified)" << std::endl;
    } else {
        std::cout << "  AVX2 differs from scalar!" << std::endl;
    }
    
    // AVX2 (fast exp)
    double energy_avx2_fast = ambereef1::calc_avx2_fast_exp(atoms, params, pairs);
    std::cout << "AVX2-fast-exp:   " << std::fixed << std::setprecision(10) << energy_avx2_fast << std::endl;
    double diff_avx2_fast = std::abs(energy_scalar - energy_avx2_fast);
    double rel_diff_avx2_fast = diff_avx2_fast / std::abs(energy_scalar);
    std::cout << "  Difference:    " << diff_avx2_fast << " (relative: " 
              << std::scientific << rel_diff_avx2_fast << ")" << std::endl;
    std::cout << "  NOTE: Fast exp approximation - accuracy may differ" << std::endl;
    
    // AVX-512 (exact exp)
    double energy_avx512 = ambereef1::calc_avx512(atoms, params, pairs);
    std::cout << "AVX-512 energy:  " << std::fixed << std::setprecision(10) << energy_avx512 << std::endl;
    double diff_avx512 = std::abs(energy_scalar - energy_avx512);
    double rel_diff_avx512 = diff_avx512 / std::abs(energy_scalar);
    std::cout << "  Difference:    " << diff_avx512 << " (relative: " 
              << std::scientific << rel_diff_avx512 << ")" << std::endl;
    if (rel_diff_avx512 < 1e-10) {
        std::cout << "  AVX-512 matches scalar (correctness verified)" << std::endl;
    } else {
        std::cout << "  AVX-512 differs from scalar!" << std::endl;
    }
    
    // AVX-512 (fast exp)
    double energy_avx512_fast = ambereef1::calc_avx512_fast_exp(atoms, params, pairs);
    std::cout << "AVX-512-fast-exp:" << std::fixed << std::setprecision(10) << energy_avx512_fast << std::endl;
    double diff_avx512_fast = std::abs(energy_scalar - energy_avx512_fast);
    double rel_diff_avx512_fast = diff_avx512_fast / std::abs(energy_scalar);
    std::cout << "  Difference:    " << diff_avx512_fast << " (relative: " 
              << std::scientific << rel_diff_avx512_fast << ")" << std::endl;
    std::cout << "  NOTE: Fast exp approximation - accuracy may differ" << std::endl;
    
    std::cout << std::endl;
    
    // Benchmark all versions
    std::cout << "=== Performance Benchmark ===" << std::endl;
    
    double time_scalar = benchmark_function(
        ambereef1::calc_scalar, atoms, params, pairs, iterations, "Scalar");
    
    double time_avx2 = benchmark_function(
        ambereef1::calc_avx2, atoms, params, pairs, iterations, "AVX2 (exact exp)");
    
    double time_avx2_fast = benchmark_function(
        ambereef1::calc_avx2_fast_exp, atoms, params, pairs, iterations, "AVX2 (fast exp)");
    
    double time_avx512 = benchmark_function(
        ambereef1::calc_avx512, atoms, params, pairs, iterations, "AVX-512 (exact exp)");
    
    double time_avx512_fast = benchmark_function(
        ambereef1::calc_avx512_fast_exp, atoms, params, pairs, iterations, "AVX-512 (fast exp)");
    
    std::cout << std::endl;
    std::cout << "=== Speedup Summary ===" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "AVX2 (exact):     " << (time_scalar / time_avx2) << "x" << std::endl;
    std::cout << "AVX2 (fast exp):  " << (time_scalar / time_avx2_fast) << "x" << std::endl;
    std::cout << "AVX-512 (exact):  " << (time_scalar / time_avx512) << "x" << std::endl;
    std::cout << "AVX-512 (fast):   " << (time_scalar / time_avx512_fast) << "x" << std::endl;
    std::cout << "AVX-512 vs AVX2 (exact): " << (time_avx2 / time_avx512) << "x" << std::endl;
#else
    std::cout << "SIMD not enabled. Rebuild with -DENABLE_SIMD=ON" << std::endl;
    return 1;
#endif
    
    return 0;
}

