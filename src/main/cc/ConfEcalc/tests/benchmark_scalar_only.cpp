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
#include "energy_ambereef1.h"
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <cassert>

using namespace osprey;
using namespace osprey::ambereef1;

int main(int argc, char* argv[]) {
    // Parse arguments: atoms amber_pairs eef1_pairs iterations
    int num_atoms = argc > 1 ? std::atoi(argv[1]) : 200;
    int num_amber = argc > 2 ? std::atoi(argv[2]) : 2000;
    int num_eef1 = argc > 3 ? std::atoi(argv[3]) : 1000;
    int iterations = argc > 4 ? std::atoi(argv[4]) : 1000;
    
    // Create test data
    // Seed RNG for reproducibility across runs/machines.
    std::srand(42);
    Array<Real3<double>> atoms(num_atoms);
    for (int i = 0; i < num_atoms; i++) {
        atoms[i] = Real3<double>(
            10.0 * (double)rand() / RAND_MAX + 0.1,
            10.0 * (double)rand() / RAND_MAX + 0.1,
            10.0 * (double)rand() / RAND_MAX + 0.1
        );
    }
    
    Params params;
    params.distance_dependent_dielectric = false;
    size_t pairs_header_size = sizeof(AtomPairs);
    size_t amber_pairs_size = num_amber * sizeof(AtomPairAmber<double>);
    size_t eef1_pairs_size = num_eef1 * sizeof(AtomPairEef1<double>);
    size_t total_size = pairs_header_size + amber_pairs_size + eef1_pairs_size;
    
    std::vector<uint8_t> pairs_buffer(total_size);
    memset(pairs_buffer.data(), 0, total_size);
    
    AtomPairs* pairs_header = reinterpret_cast<AtomPairs*>(pairs_buffer.data());
    pairs_header->num_amber = num_amber;
    pairs_header->num_eef1 = num_eef1;
    
    AtomPairAmber<double>* amber_pairs = 
        reinterpret_cast<AtomPairAmber<double>*>(pairs_buffer.data() + pairs_header_size);
    for (int j = 0; j < num_amber; j++) {
        int atom1_idx = rand() % num_atoms;
        int atom2_idx = rand() % num_atoms;
        while (atom1_idx == atom2_idx) {
            atom2_idx = rand() % num_atoms;
        }
        amber_pairs[j].atomi1 = atom1_idx;
        amber_pairs[j].atomi2 = atom2_idx;
        amber_pairs[j].esQ = 1.0 + (double)rand() / RAND_MAX;
        amber_pairs[j].vdwA = 1000.0 + 1000.0 * (double)rand() / RAND_MAX;
        amber_pairs[j].vdwB = 10.0 + 10.0 * (double)rand() / RAND_MAX;
    }
    
    AtomPairEef1<double>* eef1_pairs = 
        reinterpret_cast<AtomPairEef1<double>*>(
            pairs_buffer.data() + pairs_header_size + amber_pairs_size);
    for (int j = 0; j < num_eef1; j++) {
        int atom1_idx = rand() % num_atoms;
        int atom2_idx = rand() % num_atoms;
        while (atom1_idx == atom2_idx) {
            atom2_idx = rand() % num_atoms;
        }
        eef1_pairs[j].atomi1 = atom1_idx;
        eef1_pairs[j].atomi2 = atom2_idx;
        eef1_pairs[j].vdwRadius1 = 1.5 + 0.5 * (double)rand() / RAND_MAX;
        eef1_pairs[j].lambda1 = 0.5 + 0.2 * (double)rand() / RAND_MAX;
        eef1_pairs[j].vdwRadius2 = 1.5 + 0.5 * (double)rand() / RAND_MAX;
        eef1_pairs[j].lambda2 = 0.5 + 0.2 * (double)rand() / RAND_MAX;
        eef1_pairs[j].alpha1 = 1.0 + (double)rand() / RAND_MAX;
        eef1_pairs[j].alpha2 = 1.0 + (double)rand() / RAND_MAX;
    }
    
    AtomPairs& pairs = *pairs_header;
    
    // Warmup
    for (int i = 0; i < 10; i++) {
        (void)calc_scalar(atoms, params, pairs);
    }
    
    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    volatile double sink = 0.0;  // prevent the compiler from optimizing away the work
    for (int i = 0; i < iterations; i++) {
        sink = sink + calc_scalar(atoms, params, pairs);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    double time_us = duration.count() / (double)iterations;
    
    std::cout << time_us << " us/iter" << std::endl;
    return 0;
}

