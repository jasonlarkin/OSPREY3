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
#include <string>

#ifdef USE_SIMD
#include "../rotation_simd.h"
#include "../real3_simd.h"
#endif

using namespace osprey;

// Benchmark rotation matrix-vector multiply
void benchmark_rotation_multiply(int num_vectors, int iterations, bool run_scalar, bool run_simd) {
    std::cout << "\n=== Benchmark: Rotation Matrix-Vector Multiply ===" << std::endl;
    std::cout << "Vectors: " << num_vectors << ", Iterations: " << iterations << std::endl;
    
    // Create rotation matrix
    Rotation<double> rot;
    rot.set_xyz(0.5, 1.2, -0.8); // Arbitrary rotation
    
    // Create input vectors
    std::vector<Real3<double>> input_vectors(num_vectors);
    std::vector<Real3<double>> output_vectors_scalar(num_vectors);
    std::vector<Real3<double>> output_vectors_simd(num_vectors);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-10.0, 10.0);
    for (int i = 0; i < num_vectors; i++) {
        input_vectors[i] = Real3<double>(dist(rng), dist(rng), dist(rng));
    }
    
    long long scalar_time = 0;
    if (run_scalar) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < iterations; iter++) {
            for (int i = 0; i < num_vectors; i++) {
                output_vectors_scalar[i] = rot * input_vectors[i];
            }
        }
        auto end = std::chrono::high_resolution_clock::now();
        scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    
    // SIMD benchmark (AVX2)
#ifdef USE_SIMD
    long long simd_time = 0;
    if (run_simd) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < iterations; iter++) {
            rotate_vectors_avx2(rot, input_vectors.data(), output_vectors_simd.data(), num_vectors);
        }
        auto end = std::chrono::high_resolution_clock::now();
        simd_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    if (run_scalar) {
        std::cout << "Scalar time: " << scalar_time << " us" << std::endl;
    }
    if (run_simd) {
        std::cout << "SIMD time:   " << simd_time << " us" << std::endl;
    }

    // Verify correctness only when both were run
    if (run_scalar && run_simd) {
        double max_error = 0.0;
        for (int i = 0; i < num_vectors; i++) {
            double error = std::abs(output_vectors_scalar[i].x - output_vectors_simd[i].x) +
                           std::abs(output_vectors_scalar[i].y - output_vectors_simd[i].y) +
                           std::abs(output_vectors_scalar[i].z - output_vectors_simd[i].z);
            max_error = std::max(max_error, error);
        }
        if (simd_time > 0) {
            double speedup = (double)scalar_time / simd_time;
            std::cout << "Speedup:     " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        }
        std::cout << "Max error:   " << std::scientific << max_error << std::endl;
        if (max_error > 1e-10) {
            std::cerr << "WARNING: Large numerical error detected!" << std::endl;
        }
    }
#else
    if (run_scalar) {
        std::cout << "Scalar time: " << scalar_time << " us" << std::endl;
    }
    if (run_simd) {
        std::cout << "SIMD not enabled" << std::endl;
    }
#endif
}

// Benchmark Real3 normalize
void benchmark_normalize(int num_vectors, int iterations, bool run_scalar, bool run_simd) {
    std::cout << "\n=== Benchmark: Real3 Normalize ===" << std::endl;
    std::cout << "Vectors: " << num_vectors << ", Iterations: " << iterations << std::endl;
    
    // IMPORTANT:
    // This benchmark must use identical input data for scalar and SIMD, otherwise
    // any "Max error" check is meaningless. Keep a fixed base input and copy it
    // into working buffers for each timed run.
    std::vector<Real3<double>> base_vectors(num_vectors);
    std::vector<Real3<double>> vectors_scalar(num_vectors);
    std::vector<Real3<double>> vectors_simd(num_vectors);
    
    std::mt19937 rng(42);
    std::uniform_real_distribution<double> dist(-10.0, 10.0);
    for (int i = 0; i < num_vectors; i++) {
        double x = dist(rng);
        double y = dist(rng);
        double z = dist(rng);
        base_vectors[i] = Real3<double>(x, y, z);
    }
    
    long long scalar_time = 0;
    if (run_scalar) {
        vectors_scalar = base_vectors;
        auto start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < iterations; iter++) {
            for (int i = 0; i < num_vectors; i++) {
                vectors_scalar[i].normalize();
            }
            // Reset to identical inputs for the next iteration.
            vectors_scalar = base_vectors;
        }
        auto end = std::chrono::high_resolution_clock::now();
        scalar_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }
    
    // SIMD benchmark
#ifdef USE_SIMD
    long long simd_time = 0;
    if (run_simd) {
        vectors_simd = base_vectors;
        auto start = std::chrono::high_resolution_clock::now();
        for (int iter = 0; iter < iterations; iter++) {
            normalize_avx2(vectors_simd.data(), num_vectors);
            vectors_simd = base_vectors;
        }
        auto end = std::chrono::high_resolution_clock::now();
        simd_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
    }

    if (run_scalar) {
        std::cout << "Scalar time: " << scalar_time << " us" << std::endl;
    }
    if (run_simd) {
        std::cout << "SIMD time:   " << simd_time << " us" << std::endl;
    }

    // Verify correctness only when both were run
    if (run_scalar && run_simd) {
        vectors_scalar = base_vectors;
        vectors_simd = base_vectors;
        for (int i = 0; i < num_vectors; i++) {
            vectors_scalar[i].normalize();
        }
        normalize_avx2(vectors_simd.data(), num_vectors);

        double max_error = 0.0;
        for (int i = 0; i < num_vectors; i++) {
            double error =
                std::abs(vectors_scalar[i].x - vectors_simd[i].x) +
                std::abs(vectors_scalar[i].y - vectors_simd[i].y) +
                std::abs(vectors_scalar[i].z - vectors_simd[i].z);
            max_error = std::max(max_error, error);
        }

        if (simd_time > 0) {
            double speedup = (double)scalar_time / simd_time;
            std::cout << "Speedup:     " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
        }
        std::cout << "Max error:   " << std::scientific << max_error << std::endl;
    }
#else
    if (run_scalar) {
        std::cout << "Scalar time: " << scalar_time << " us" << std::endl;
    }
    if (run_simd) {
        std::cout << "SIMD not enabled" << std::endl;
    }
#endif
}

int main(int argc, char* argv[]) {
    // Arguments:
    //   benchmark_rotation_operations <num_vectors> <iterations> [--op rotation|normalize|both] [--impl scalar|simd|both]
    // Defaults: op=both, impl=both
    int num_vectors = 100;
    int iterations = 10000;
    std::string op = "both";
    std::string impl = "both";

    // Positional parsing for backwards-compatibility
    int positional_seen = 0;
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--op" && i + 1 < argc) {
            op = argv[++i];
        } else if (arg == "--impl" && i + 1 < argc) {
            impl = argv[++i];
        } else if (!arg.empty() && arg[0] != '-') {
            // positional ints
            if (positional_seen == 0) {
                num_vectors = std::atoi(arg.c_str());
                positional_seen++;
            } else if (positional_seen == 1) {
                iterations = std::atoi(arg.c_str());
                positional_seen++;
            }
        }
    }

    bool run_rotation = (op == "rotation" || op == "both");
    bool run_normalize = (op == "normalize" || op == "both");
    bool run_scalar = (impl == "scalar" || impl == "both");
    bool run_simd = (impl == "simd" || impl == "both");
    
    std::cout << "=== Rotation Operations Benchmark ===" << std::endl;
    std::cout << "Number of vectors: " << num_vectors << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Op: " << op << std::endl;
    std::cout << "Impl: " << impl << std::endl;
    
    if (run_rotation) {
        benchmark_rotation_multiply(num_vectors, iterations, run_scalar, run_simd);
    }
    
    if (run_normalize) {
        benchmark_normalize(num_vectors, iterations, run_scalar, run_simd);
    }
    
    return 0;
}

