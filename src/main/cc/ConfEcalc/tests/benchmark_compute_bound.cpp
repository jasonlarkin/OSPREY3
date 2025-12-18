// Compute-bound benchmark: Dense matrix multiplication
// This demonstrates high arithmetic intensity (compute-bound workload)
// Usage: ./benchmark_compute_bound <matrix_size> <iterations>

#include <iostream>
#include <vector>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <cstring>
#include <immintrin.h>

#ifdef __AVX512F__
#include <immintrin.h>
#endif

// Scalar matrix multiplication
void matmul_scalar(const std::vector<double>& A, const std::vector<double>& B, 
                   std::vector<double>& C, int n) {
    for (int i = 0; i < n; i++) {
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) {
                sum += A[i * n + k] * B[k * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

// AVX2 matrix multiplication (blocked for cache efficiency)
void matmul_avx2(const std::vector<double>& A, const std::vector<double>& B,
                 std::vector<double>& C, int n) {
    const int block_size = 64;  // Cache blocking
    
    for (int ii = 0; ii < n; ii += block_size) {
        for (int jj = 0; jj < n; jj += block_size) {
            for (int kk = 0; kk < n; kk += block_size) {
                // Process block
                for (int i = ii; i < std::min(ii + block_size, n); i++) {
                    for (int j = jj; j < std::min(jj + block_size, n); j += 4) {
                        __m256d sum = _mm256_setzero_pd();
                        for (int k = kk; k < std::min(kk + block_size, n); k++) {
                            __m256d a = _mm256_broadcast_sd(&A[i * n + k]);
                            __m256d b = _mm256_loadu_pd(&B[k * n + j]);
                            sum = _mm256_fmadd_pd(a, b, sum);
                        }
                        if (j + 4 <= n) {
                            _mm256_storeu_pd(&C[i * n + j], sum);
                        } else {
                            // Handle remainder
                            double temp[4];
                            _mm256_storeu_pd(temp, sum);
                            for (int rem = 0; rem < n - j; rem++) {
                                C[i * n + j + rem] = temp[rem];
                            }
                        }
                    }
                }
            }
        }
    }
}

#ifdef __AVX512F__
// AVX-512 matrix multiplication
void matmul_avx512(const std::vector<double>& A, const std::vector<double>& B,
                   std::vector<double>& C, int n) {
    const int block_size = 64;
    
    for (int ii = 0; ii < n; ii += block_size) {
        for (int jj = 0; jj < n; jj += block_size) {
            for (int kk = 0; kk < n; kk += block_size) {
                for (int i = ii; i < std::min(ii + block_size, n); i++) {
                    for (int j = jj; j < std::min(jj + block_size, n); j += 8) {
                        __m512d sum = _mm512_setzero_pd();
                        for (int k = kk; k < std::min(kk + block_size, n); k++) {
                            __m512d a = _mm512_set1_pd(A[i * n + k]);
                            __m512d b = _mm512_loadu_pd(&B[k * n + j]);
                            sum = _mm512_fmadd_pd(a, b, sum);
                        }
                        if (j + 8 <= n) {
                            _mm512_storeu_pd(&C[i * n + j], sum);
                        } else {
                            double temp[8];
                            _mm512_storeu_pd(temp, sum);
                            for (int rem = 0; rem < n - j; rem++) {
                                C[i * n + j + rem] = temp[rem];
                            }
                        }
                    }
                }
            }
        }
    }
}
#endif

int main(int argc, char* argv[]) {
    int matrix_size = 512;
    int iterations = 10;
    
    if (argc >= 2) {
        matrix_size = std::atoi(argv[1]);
    }
    if (argc >= 3) {
        iterations = std::atoi(argv[2]);
    }
    
    std::cout << "=== Compute-Bound Benchmark: Matrix Multiplication ===" << std::endl;
    std::cout << "Matrix size: " << matrix_size << "x" << matrix_size << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << std::endl;
    
    // Allocate matrices
    std::vector<double> A(matrix_size * matrix_size, 1.0);
    std::vector<double> B(matrix_size * matrix_size, 1.0);
    std::vector<double> C(matrix_size * matrix_size, 0.0);
    
    // Calculate FLOPs: 2*n^3 per matrix multiply
    long long flops_per_iter = 2LL * matrix_size * matrix_size * matrix_size;
    long long total_flops = flops_per_iter * iterations;
    
    // Calculate memory traffic: 3 matrices (read A, read B, write C)
    long long bytes_per_iter = 3 * matrix_size * matrix_size * sizeof(double);
    long long total_bytes = bytes_per_iter * iterations;
    
    double arithmetic_intensity = (double)total_flops / total_bytes;
    
    std::cout << "FLOPs per iteration: " << flops_per_iter / 1e9 << " GFLOPs" << std::endl;
    std::cout << "Bytes per iteration: " << bytes_per_iter / 1e9 << " GB" << std::endl;
    std::cout << "Arithmetic Intensity: " << arithmetic_intensity << " FLOPs/byte" << std::endl;
    std::cout << std::endl;
    
    // Benchmark scalar
    {
        std::cout << "Benchmarking scalar..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            matmul_scalar(A, B, C, matrix_size);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double time_sec = duration.count() / 1e6;
        double gflops = (total_flops / time_sec) / 1e9;
        std::cout << "  Time: " << std::fixed << std::setprecision(3) << time_sec << " s" << std::endl;
        std::cout << "  Performance: " << gflops << " GFLOP/s" << std::endl;
        std::cout << std::endl;
    }
    
    // Benchmark AVX2
    {
        std::cout << "Benchmarking AVX2..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            matmul_avx2(A, B, C, matrix_size);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double time_sec = duration.count() / 1e6;
        double gflops = (total_flops / time_sec) / 1e9;
        std::cout << "  Time: " << std::fixed << std::setprecision(3) << time_sec << " s" << std::endl;
        std::cout << "  Performance: " << gflops << " GFLOP/s" << std::endl;
        std::cout << std::endl;
    }
    
#ifdef __AVX512F__
    // Benchmark AVX-512
    {
        std::cout << "Benchmarking AVX-512..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < iterations; i++) {
            matmul_avx512(A, B, C, matrix_size);
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        double time_sec = duration.count() / 1e6;
        double gflops = (total_flops / time_sec) / 1e9;
        std::cout << "  Time: " << std::fixed << std::setprecision(3) << time_sec << " s" << std::endl;
        std::cout << "  Performance: " << gflops << " GFLOP/s" << std::endl;
        std::cout << std::endl;
    }
#endif
    
    std::cout << "=== Summary ===" << std::endl;
    std::cout << "This workload is COMPUTE-BOUND (high arithmetic intensity)" << std::endl;
    std::cout << "AI = " << arithmetic_intensity << " FLOPs/byte >> 2.0 (compute/memory boundary)" << std::endl;
    
    return 0;
}

