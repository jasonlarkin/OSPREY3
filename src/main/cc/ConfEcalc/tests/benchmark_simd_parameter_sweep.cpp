/*
 * Parameter sweep benchmark for SIMD energy calculations
 * 
 * This benchmark sweeps different parameter ranges to characterize
 * performance and identify memory vs compute bottlenecks.
 * 
 * Usage:
 *   ./benchmark_simd_parameter_sweep [iterations] > results.csv
 *   # Then analyze with: perf stat, perf record, or Intel VTune
 */

#include "../global.h"
#include "../array.h"
#include "../formats.h"
#include "../real3.h"
#include "../atoms.h"
#include "../confspace.h"
#include "../assignment.h"
#include "../energy.h"
#include "../energy_ambereef1.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <cmath>
#include <cassert>
#include <vector>
#include <cstring>
#include <cstdint>

using namespace osprey;

// Create test data with specified parameters
void create_test_data(
    Array<Real3<double>>& atoms,
    ambereef1::Params& params,
    std::vector<uint8_t>& pairs_buffer,
    int num_atoms,
    int num_amber_pairs,
    int num_eef1_pairs
) {
    // Spread atoms in a 3D grid
    srand(42);  // Seed for reproducibility
    for (int i = 0; i < num_atoms; i++) {
        atoms[i] = Real3<double>(
            0.1 + 1.0 * (double)(i % 10),
            0.1 + 1.0 * (double)((i / 10) % 10),
            0.1 + 1.0 * (double)(i / 100)
        );
    }
    
    params.distance_dependent_dielectric = false;
    
    size_t pairs_header_size = sizeof(ambereef1::AtomPairs);
    size_t amber_pairs_size = num_amber_pairs * sizeof(ambereef1::AtomPairAmber<double>);
    size_t eef1_pairs_size = num_eef1_pairs * sizeof(ambereef1::AtomPairEef1<double>);
    size_t total_size = pairs_header_size + amber_pairs_size + eef1_pairs_size;
    
    pairs_buffer.resize(total_size);
    memset(pairs_buffer.data(), 0, total_size);
    
    ambereef1::AtomPairs* pairs_header = reinterpret_cast<ambereef1::AtomPairs*>(pairs_buffer.data());
    pairs_header->num_amber = num_amber_pairs;
    pairs_header->num_eef1 = num_eef1_pairs;
    
    // Create amber pairs
    ambereef1::AtomPairAmber<double>* amber_pairs = 
        reinterpret_cast<ambereef1::AtomPairAmber<double>*>(pairs_buffer.data() + pairs_header_size);
    for (int i = 0; i < num_amber_pairs; i++) {
        int a1 = rand() % num_atoms;
        int a2;
        do {
            a2 = rand() % num_atoms;
        } while (a2 == a1);
        amber_pairs[i].atomi1 = a1;
        amber_pairs[i].atomi2 = a2;
        amber_pairs[i].esQ = 1.0 + 10.0 * (double)rand() / RAND_MAX;
        amber_pairs[i].vdwA = 1000.0 + 1000.0 * (double)rand() / RAND_MAX;
        amber_pairs[i].vdwB = 10.0 + 10.0 * (double)rand() / RAND_MAX;
    }
    
    // Create eef1 pairs
    ambereef1::AtomPairEef1<double>* eef1_pairs = 
        reinterpret_cast<ambereef1::AtomPairEef1<double>*>(
            pairs_buffer.data() + pairs_header_size + amber_pairs_size);
    for (int i = 0; i < num_eef1_pairs; i++) {
        int a1 = rand() % num_atoms;
        int a2;
        do {
            a2 = rand() % num_atoms;
        } while (a2 == a1);
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

// Benchmark a single function and return time in microseconds
double benchmark_function(
    double (*calc_func)(const Array<Real3<double>>&, const ambereef1::Params&, const ambereef1::AtomPairs&),
    const Array<Real3<double>>& atoms,
    const ambereef1::Params& params,
    const ambereef1::AtomPairs& pairs,
    int iterations
) {
    // Warmup
    for (int i = 0; i < 10; i++) {
        calc_func(atoms, params, pairs);
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    volatile double total_energy = 0.0;
    for (int i = 0; i < iterations; i++) {
        total_energy += calc_func(atoms, params, pairs);
    }
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    return (double)duration.count() / iterations;
}

// Calculate theoretical memory bandwidth requirements
double calculate_memory_ops(int num_atoms, int num_amber_pairs, int num_eef1_pairs) {
    // Read atoms: 3 doubles per atom per pair = 24 bytes per atom per pair
    // Read pairs: ~32 bytes per pair (average of amber and eef1)
    // Total per iteration: pairs * (atom_reads + pair_data)
    double bytes_per_iter = (num_amber_pairs + num_eef1_pairs) * (24.0 + 32.0);
    return bytes_per_iter / (1024.0 * 1024.0);  // MB per iteration
}

// Calculate compute intensity (FLOPs per byte)
double calculate_compute_intensity(int num_amber_pairs, int num_eef1_pairs) {
    // Rough estimate: per pair we do ~20-30 FLOPs (distance calc, energy calc)
    // Memory: ~56 bytes per pair
    double flops_per_pair = 25.0;  // Average estimate
    double bytes_per_pair = 56.0;
    return flops_per_pair / bytes_per_pair;
}

int main(int argc, char* argv[]) {
    int iterations = 1000;
    if (argc > 1) {
        iterations = std::atoi(argv[1]);
    }
    
    // Print CSV header
    std::cout << "# Parameter Sweep Benchmark Results" << std::endl;
    std::cout << "# Format: num_atoms,num_amber_pairs,num_eef1_pairs,"
              << "total_pairs,mem_ops_mb,compute_intensity,"
              << "scalar_us,avx2_us,avx512_us,"
              << "avx2_speedup,avx512_speedup,avx512_vs_avx2" << std::endl;
    
    // Parameter ranges to sweep
    std::vector<int> atom_counts = {50, 100, 200, 500, 1000};
    std::vector<int> amber_pair_counts = {100, 500, 1000, 2000, 5000, 10000};
    std::vector<int> eef1_pair_counts = {50, 250, 500, 1000, 2500, 5000};
    
#ifdef USE_SIMD
    for (int num_atoms : atom_counts) {
        for (int num_amber : amber_pair_counts) {
            for (int num_eef1 : eef1_pair_counts) {
                if (num_amber + num_eef1 > 20000) continue;  // Skip very large cases
                
                // Create test data
                Array<Real3<double>> atoms(num_atoms);
                ambereef1::Params params;
                std::vector<uint8_t> pairs_buffer;
                create_test_data(atoms, params, pairs_buffer, num_atoms, num_amber, num_eef1);
                const ambereef1::AtomPairs& pairs = *reinterpret_cast<const ambereef1::AtomPairs*>(pairs_buffer.data());
                
                // Benchmark all versions
                double time_scalar = benchmark_function(
                    ambereef1::calc_scalar, atoms, params, pairs, iterations);
                double time_avx2 = benchmark_function(
                    ambereef1::calc_avx2, atoms, params, pairs, iterations);
                double time_avx512 = benchmark_function(
                    ambereef1::calc_avx512, atoms, params, pairs, iterations);
                
                // Calculate metrics
                int total_pairs = num_amber + num_eef1;
                double mem_ops = calculate_memory_ops(num_atoms, num_amber, num_eef1);
                double comp_intensity = calculate_compute_intensity(num_amber, num_eef1);
                double avx2_speedup = time_scalar / time_avx2;
                double avx512_speedup = time_scalar / time_avx512;
                double avx512_vs_avx2 = time_avx2 / time_avx512;
                
                // Output CSV
                std::cout << std::fixed << std::setprecision(2);
                std::cout << num_atoms << ","
                          << num_amber << ","
                          << num_eef1 << ","
                          << total_pairs << ","
                          << std::setprecision(4) << mem_ops << ","
                          << comp_intensity << ","
                          << std::setprecision(2) << time_scalar << ","
                          << time_avx2 << ","
                          << time_avx512 << ","
                          << std::setprecision(4) << avx2_speedup << ","
                          << avx512_speedup << ","
                          << avx512_vs_avx2 << std::endl;
                
                std::cerr << "Completed: " << num_atoms << " atoms, "
                          << num_amber << "+" << num_eef1 << " pairs" << std::endl;
            }
        }
    }
#else
    std::cerr << "SIMD not enabled. Rebuild with -DENABLE_SIMD=ON" << std::endl;
    return 1;
#endif
    
    return 0;
}

