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
#include "minimization.h"
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

#ifdef USE_OPENMP
#include <omp.h>
#endif

using namespace osprey;
using namespace osprey::ambereef1;

// Simplified benchmark that simulates the DOF loop pattern
// to measure threading overhead and critical section impact
struct BenchmarkResult {
    double total_time_ms;
    double line_search_time_ms;
    double overhead_time_ms;
    int num_dofs;
    int num_iterations;
    bool threaded;
};

// Simulate energy evaluation time
template<typename T>
T simulate_energy_eval(int complexity) {
    volatile T sum = 0.0;
    for (int i = 0; i < complexity; i++) {
        sum += std::sin(static_cast<T>(i)) * std::cos(static_cast<T>(i));
    }
    return sum;
}

// Simulate line_search pattern (simplified)
template<typename T>
T simulate_line_search(int dof_idx, T x_init, T& step, int energy_complexity, bool use_critical = false) {
    // Simulate multiple energy evaluations like real line_search does
    T x = x_init;
    T best_x = x;
    T best_f = std::numeric_limits<T>::max();
    
    // Simulate quadratic fit (3 evaluations)
    for (int i = 0; i < 3; i++) {
        T test_x = x + (i - 1) * step;
        
#ifdef USE_OPENMP
        if (use_critical) {
            #pragma omp critical(minimize_ccd_line_search)
            {
                best_f = simulate_energy_eval<T>(energy_complexity);
            }
        } else {
            best_f = simulate_energy_eval<T>(energy_complexity);
        }
#else
        best_f = simulate_energy_eval<T>(energy_complexity);
#endif
        
        T test_f = simulate_energy_eval<T>(energy_complexity);
        if (test_f < best_f) {
            best_x = test_x;
            best_f = test_f;
        }
    }
    
    // Simulate surfing (additional evaluations)
    for (int i = 0; i < 5; i++) {
        T test_x = x + step * (1 << i);
        
#ifdef USE_OPENMP
        if (use_critical) {
            #pragma omp critical(minimize_ccd_line_search)
            {
                simulate_energy_eval<T>(energy_complexity / 2);
            }
        } else {
            simulate_energy_eval<T>(energy_complexity / 2);
        }
#else
        simulate_energy_eval<T>(energy_complexity / 2);
#endif
    }
    
    return best_x;
}

// Benchmark the DOF loop pattern (simulates minimize_ccd loop)
template<typename T>
BenchmarkResult benchmark_dof_loop_pattern(int num_dofs, int num_iterations, 
                                           int energy_complexity, bool threaded, 
                                           bool use_critical_section) {
    BenchmarkResult result;
    result.num_dofs = num_dofs;
    result.num_iterations = num_iterations;
    result.threaded = threaded;
    
    std::vector<T> dof_values(num_dofs, 0.5);
    std::vector<T> step_sizes(num_dofs, 0.1);
    
    auto start_total = std::chrono::high_resolution_clock::now();
    auto start_line_search = std::chrono::high_resolution_clock::now();
    
    if (threaded) {
#ifdef USE_OPENMP
        #pragma omp parallel for schedule(static) default(none) \
            shared(dof_values, step_sizes, num_iterations, energy_complexity, use_critical_section, num_dofs)
        for (int d = 0; d < num_dofs; d++) {
            for (int iter = 0; iter < num_iterations; iter++) {
                // Simulate step size calculation (parallelizable)
                T step = step_sizes[d] / std::pow(iter + 1, 3);
                
                // Simulate line_search (may need critical section)
                dof_values[d] = simulate_line_search<T>(d, dof_values[d], step, 
                                                         energy_complexity, 
                                                         use_critical_section);
                
                // Update step size
                step_sizes[d] = step;
            }
        }
#else
        // OpenMP not available, run sequentially
        threaded = false;
        for (int d = 0; d < num_dofs; d++) {
            for (int iter = 0; iter < num_iterations; iter++) {
                T step = step_sizes[d] / std::pow(iter + 1, 3);
                dof_values[d] = simulate_line_search<T>(d, dof_values[d], step, 
                                                         energy_complexity, false);
                step_sizes[d] = step;
            }
        }
#endif
    } else {
        // Sequential version
        for (int d = 0; d < num_dofs; d++) {
            for (int iter = 0; iter < num_iterations; iter++) {
                T step = step_sizes[d] / std::pow(iter + 1, 3);
                dof_values[d] = simulate_line_search<T>(d, dof_values[d], step, 
                                                         energy_complexity, false);
                step_sizes[d] = step;
            }
        }
    }
    
    auto end_line_search = std::chrono::high_resolution_clock::now();
    auto end_total = std::chrono::high_resolution_clock::now();
    
    result.line_search_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        end_line_search - start_line_search).count() / 1000.0;
    result.total_time_ms = std::chrono::duration_cast<std::chrono::microseconds>(
        end_total - start_total).count() / 1000.0;
    result.overhead_time_ms = result.total_time_ms - result.line_search_time_ms;
    
    // Prevent optimization
    volatile T sum = 0;
    for (int d = 0; d < num_dofs; d++) {
        sum += dof_values[d];
    }
    (void)sum;
    
    return result;
}

void print_results(const BenchmarkResult& seq, const BenchmarkResult& thread_no_critical, 
                   const BenchmarkResult& thread_with_critical) {
    std::cout << std::fixed << std::setprecision(3);
    
    std::cout << "\n=== Benchmark Results ===" << std::endl;
    std::cout << "DOFs: " << seq.num_dofs << ", Iterations: " << seq.num_iterations << std::endl;
    std::cout << "\nSequential (no threading):" << std::endl;
    std::cout << "  Total time: " << seq.total_time_ms << " ms" << std::endl;
    std::cout << "  Time per DOF*iteration: " 
              << seq.total_time_ms / (seq.num_dofs * seq.num_iterations) << " ms" << std::endl;
    
#ifdef USE_OPENMP
    if (thread_no_critical.threaded) {
        std::cout << "\nThreaded (no critical section):" << std::endl;
        std::cout << "  Total time: " << thread_no_critical.total_time_ms << " ms" << std::endl;
        std::cout << "  Speedup: " << seq.total_time_ms / thread_no_critical.total_time_ms << "x" << std::endl;
        std::cout << "  Time per DOF*iteration: " 
                  << thread_no_critical.total_time_ms / (thread_no_critical.num_dofs * thread_no_critical.num_iterations) 
                  << " ms" << std::endl;
    }
    
    if (thread_with_critical.threaded) {
        std::cout << "\nThreaded (with critical section):" << std::endl;
        std::cout << "  Total time: " << thread_with_critical.total_time_ms << " ms" << std::endl;
        std::cout << "  Speedup: " << seq.total_time_ms / thread_with_critical.total_time_ms << "x" << std::endl;
        std::cout << "  Overhead vs no-critical: " 
                  << thread_with_critical.total_time_ms / thread_no_critical.total_time_ms << "x slower" << std::endl;
        std::cout << "  Time per DOF*iteration: " 
                  << thread_with_critical.total_time_ms / (thread_with_critical.num_dofs * thread_with_critical.num_iterations) 
                  << " ms" << std::endl;
    }
    
    std::cout << "\n=== Analysis ===" << std::endl;
    if (thread_no_critical.threaded && thread_with_critical.threaded) {
        double critical_overhead = (thread_with_critical.total_time_ms / thread_no_critical.total_time_ms - 1.0) * 100.0;
        std::cout << "Critical section overhead: " << std::setprecision(1) << critical_overhead << "%" << std::endl;
        
        double seq_vs_thread_speedup = seq.total_time_ms / thread_with_critical.total_time_ms;
        if (seq_vs_thread_speedup > 1.1) {
            std::cout << "Threading beneficial: " << seq_vs_thread_speedup << "x speedup" << std::endl;
        } else if (seq_vs_thread_speedup < 0.9) {
            std::cout << "Threading harmful: " << seq_vs_thread_speedup << "x (overhead too high)" << std::endl;
        } else {
            std::cout << "Threading neutral: " << seq_vs_thread_speedup << "x (overhead cancels benefit)" << std::endl;
        }
    }
#else
    std::cout << "\nOpenMP not available - cannot test threaded version" << std::endl;
#endif
}

int main(int argc, char* argv[]) {
    // Parse arguments: num_dofs iterations energy_complexity
    // Typical values:
    // - Small: 10 DOFs, 30 iterations
    // - Medium: 50 DOFs, 30 iterations  
    // - Large: 100+ DOFs, 30 iterations
    int num_dofs = argc > 1 ? std::atoi(argv[1]) : 50;
    int num_iterations = argc > 2 ? std::atoi(argv[2]) : 30;
    int energy_complexity = argc > 3 ? std::atoi(argv[3]) : 1000; // Simulates energy eval cost
    
    std::cout << "=== CCD Minimization Threading Benchmark ===" << std::endl;
    std::cout << "Number of DOFs: " << num_dofs << std::endl;
    std::cout << "Iterations per DOF: " << num_iterations << std::endl;
    std::cout << "Energy evaluation complexity: " << energy_complexity << std::endl;
#ifdef USE_OPENMP
    std::cout << "OpenMP available: Yes (max threads: " << omp_get_max_threads() << ")" << std::endl;
#else
    std::cout << "OpenMP available: No" << std::endl;
#endif
    
    // Warmup
    std::cout << "\nWarming up..." << std::endl;
    benchmark_dof_loop_pattern<double>(10, 5, 100, false, false);
    
    std::cout << "\nRunning benchmarks..." << std::endl;
    
    // Sequential benchmark
    auto seq_result = benchmark_dof_loop_pattern<double>(num_dofs, num_iterations, 
                                                          energy_complexity, false, false);
    
    BenchmarkResult thread_no_critical;
    BenchmarkResult thread_with_critical;
    
#ifdef USE_OPENMP
    // Threaded without critical section (best case - but not thread-safe in real code)
    thread_no_critical = benchmark_dof_loop_pattern<double>(num_dofs, num_iterations, 
                                                             energy_complexity, true, false);
    
    // Threaded with critical section (realistic - thread-safe)
    thread_with_critical = benchmark_dof_loop_pattern<double>(num_dofs, num_iterations, 
                                                               energy_complexity, true, true);
#else
    thread_no_critical.threaded = false;
    thread_with_critical.threaded = false;
#endif
    
    print_results(seq_result, thread_no_critical, thread_with_critical);
    
    std::cout << "\n=== Recommendations ===" << std::endl;
    if (thread_with_critical.threaded) {
        double speedup = seq_result.total_time_ms / thread_with_critical.total_time_ms;
        if (speedup > 1.2) {
            std::cout << "Threading recommended: Significant speedup expected (" << speedup << "x)" << std::endl;
        } else if (speedup > 1.05) {
            std::cout << "Threading may help: Modest speedup expected (" << speedup << "x)" << std::endl;
        } else {
            std::cout << "Threading not recommended: Critical section overhead too high" << std::endl;
            std::cout << "  Consider: Per-thread assignment copies or DOF dependency analysis" << std::endl;
        }
    } else {
        std::cout << "Cannot evaluate threading - OpenMP not available" << std::endl;
    }
    
    return 0;
}

