cmake_minimum_required(VERSION 3.20)
project(osprey_kstar_parallel VERSION 0.1 DESCRIPTION "C++20/23 parallel K* implementation for OSPREY")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for tooling (clang-tidy, IDEs).
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(KSTAR_ENABLE_COVERAGE "Enable GCC/Clang coverage instrumentation for kstar tests (gcov/lcov)" OFF)
option(KSTAR_ENABLE_LLVM_COVERAGE "Enable Clang source-based coverage instrumentation for kstar tests (llvm-profdata/llvm-cov)" OFF)
option(KSTAR_ENABLE_NATIVE_OPT "Enable -march=native (disable for portability)" ON)
option(KSTAR_ENABLE_FUZZING "Enable libFuzzer-based fuzz targets (requires Clang)" OFF)
option(KSTAR_ENABLE_SANITIZERS "Enable ASan/UBSan for kstar code/tests (use a dedicated build dir)" OFF)
option(KSTAR_ENABLE_BENCHMARKS "Build kstar microbenchmarks (Google Benchmark)" OFF)
option(KSTAR_ENABLE_PYTHON_BINDINGS "Build Python bindings (pybind11)" OFF)
option(KSTAR_ENABLE_SIMD_INTRINSICS "Enable optional SIMD intrinsics in hot loops (build-time toggle for profiling)" ON)

if (KSTAR_ENABLE_COVERAGE AND KSTAR_ENABLE_LLVM_COVERAGE)
    message(FATAL_ERROR "Enable only one coverage mode: KSTAR_ENABLE_COVERAGE (gcov/lcov) OR KSTAR_ENABLE_LLVM_COVERAGE (llvm-cov).")
endif()
if (KSTAR_ENABLE_LLVM_COVERAGE AND KSTAR_ENABLE_FUZZING)
    message(FATAL_ERROR "KSTAR_ENABLE_LLVM_COVERAGE is not supported with KSTAR_ENABLE_FUZZING in the same build tree. Use separate build directories.")
endif()

function(kstar_apply_default_warnings target_name)
    target_compile_options(${target_name} PRIVATE -Wall -Wextra)
endfunction()

function(kstar_apply_coverage target_name)
    if (KSTAR_ENABLE_COVERAGE)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${target_name} PRIVATE -O0 -g --coverage)
            target_link_options(${target_name} PRIVATE --coverage)
        else()
            message(WARNING "KSTAR_ENABLE_COVERAGE is ON but compiler '${CMAKE_CXX_COMPILER_ID}' is not GNU/Clang; coverage flags not applied")
        endif()
    endif()
endfunction()

function(kstar_apply_llvm_coverage target_name)
    if (KSTAR_ENABLE_LLVM_COVERAGE)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target_name} PRIVATE -O0 -g -fprofile-instr-generate -fcoverage-mapping)
            target_link_options(${target_name} PRIVATE -fprofile-instr-generate -fcoverage-mapping)
        else()
            message(WARNING "KSTAR_ENABLE_LLVM_COVERAGE is ON but compiler '${CMAKE_CXX_COMPILER_ID}' is not Clang; llvm coverage flags not applied")
        endif()
    endif()
endfunction()

function(kstar_apply_sanitizers target_name)
    if (KSTAR_ENABLE_SANITIZERS)
        if (CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            # IMPORTANT: sanitizer-instrumented objects reference ASan/UBSan runtime symbols.
            # Propagate sanitizer link/compile flags to dependents so executables link correctly.
            target_compile_options(${target_name} PUBLIC -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
            target_link_options(${target_name} PUBLIC -fsanitize=address,undefined)
        else()
            message(WARNING "KSTAR_ENABLE_SANITIZERS is ON but compiler '${CMAKE_CXX_COMPILER_ID}' is not GNU/Clang; sanitizer flags not applied")
        endif()
    endif()

    if (KSTAR_ENABLE_FUZZING)
        if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            target_compile_options(${target_name} PRIVATE -O1 -g -fno-omit-frame-pointer -fsanitize=address,undefined)
            target_link_options(${target_name} PRIVATE -fsanitize=address,undefined)

            # IMPORTANT: instrument code for libFuzzer coverage without linking the fuzzer runtime.
            # The fuzzer executable will link with `-fsanitize=fuzzer`, but libraries should use `fuzzer-no-link`.
            target_compile_options(${target_name} PRIVATE -fsanitize=fuzzer-no-link)
        else()
            message(WARNING "KSTAR_ENABLE_FUZZING is ON but compiler '${CMAKE_CXX_COMPILER_ID}' is not Clang; sanitizer flags not applied")
        endif()
    endif()
endfunction()

function(kstar_apply_perf_opts target_name)
    if (NOT KSTAR_ENABLE_COVERAGE AND NOT KSTAR_ENABLE_LLVM_COVERAGE)
        target_compile_options(${target_name} PRIVATE -O3)
        if (KSTAR_ENABLE_NATIVE_OPT)
            target_compile_options(${target_name} PRIVATE -march=native)
        endif()
    endif()
endfunction()

# Source files
set(SOURCES
    kstar_parallel.cpp
    kstar_workflow.cpp
    partition_function.cpp
    thread_pool.cpp
    energy_matrix.cpp
    energy_matrix_loader.cpp
    astar_search.cpp
    astar_search_fast.cpp
    conf_search_astar.cpp
)

# Headers
set(HEADERS
    kstar_parallel.hpp
    kstar_workflow.hpp
    partition_function.hpp
    thread_pool.hpp
    sequence.hpp
    energy_matrix.hpp
    energy_matrix_loader.hpp
    conf_index.hpp
    conf_ranker.hpp
    conf_search.hpp
    conf_search_astar.hpp
    conf_search_cache.hpp
    kstar_score.hpp
    astar_node.hpp
    astar_search.hpp
    astar_node_fast.hpp
    astar_search_fast.hpp
)

# Create library
add_library(osprey_kstar_parallel STATIC ${SOURCES} ${HEADERS})
kstar_apply_default_warnings(osprey_kstar_parallel)
kstar_apply_perf_opts(osprey_kstar_parallel)
kstar_apply_coverage(osprey_kstar_parallel)
kstar_apply_llvm_coverage(osprey_kstar_parallel)
kstar_apply_sanitizers(osprey_kstar_parallel)

if (KSTAR_ENABLE_SIMD_INTRINSICS)
    target_compile_definitions(osprey_kstar_parallel PUBLIC OSPREY_KSTAR_ENABLE_SIMD_INTRINSICS=1)
else()
    target_compile_definitions(osprey_kstar_parallel PUBLIC OSPREY_KSTAR_ENABLE_SIMD_INTRINSICS=0)
endif()

# Include directories
# REPO_ROOT already calculated above for test file
get_filename_component(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ ABSOLUTE)
target_include_directories(osprey_kstar_parallel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${REPO_ROOT}/src/main/cc/ConfEcalc
)

# Link libraries
target_link_libraries(osprey_kstar_parallel
    pthread
)

# When building Python bindings, the static library must be compiled with position-independent code
# because it will be linked into a shared library (the Python module)
if (KSTAR_ENABLE_PYTHON_BINDINGS)
    set_target_properties(osprey_kstar_parallel PROPERTIES POSITION_INDEPENDENT_CODE ON)
endif()

# -----------------------------
# Python bindings (pybind11, optional)
# -----------------------------
if (KSTAR_ENABLE_PYTHON_BINDINGS)
    find_package(pybind11 QUIET)
    if (NOT pybind11_FOUND)
        # Fallback: use FetchContent if pybind11 is not installed
        include(FetchContent)
        FetchContent_Declare(
            pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v2.11.1
        )
        FetchContent_MakeAvailable(pybind11)
    endif()

    pybind11_add_module(kstar_cpp
        python_bindings.cpp
    )

    target_link_libraries(kstar_cpp PRIVATE
        osprey_kstar_parallel
        pybind11::module
    )

    target_include_directories(kstar_cpp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cc/ConfEcalc
    )

    kstar_apply_default_warnings(kstar_cpp)
    kstar_apply_perf_opts(kstar_cpp)

    # Python bindings should not use sanitizers (causes issues with Python runtime)
    # Coverage is also problematic with Python extensions
    # So we skip those for the Python module
endif()

# -----------------------------
# GoogleTest (optional, for kstar tests)
# -----------------------------
include(CTest)
if (BUILD_TESTING)
    enable_testing()
    # Avoid running gtest binaries during the build (default POST_BUILD discovery).
    # This keeps instrumented coverage builds quieter (no gcov timestamp warnings during build)
    # and makes test discovery happen when ctest runs instead.
    set(CMAKE_GTEST_DISCOVER_TESTS_DISCOVERY_MODE PRE_TEST)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

# -----------------------------
# Google Benchmark (optional, for kstar microbenchmarks)
# -----------------------------
if (KSTAR_ENABLE_BENCHMARKS)
    include(FetchContent)
    # Keep benchmark dependency light: no internal tests, no gtest integration.
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_DOWNLOAD_DEPENDENCIES ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googlebenchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG v1.8.3
    )
    FetchContent_MakeAvailable(googlebenchmark)
endif()

# -----------------------------
# Fuzzing (libFuzzer) targets
# -----------------------------
if (KSTAR_ENABLE_FUZZING)
    if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "KSTAR_ENABLE_FUZZING requires Clang (libFuzzer). Configure with: -DCMAKE_CXX_COMPILER=clang++")
    endif()

    # EnergyMatrixLoader fuzz target
    set(FUZZ_EMAT_LOADER_FILE ${REPO_ROOT}/src/test/cpp/kstar/fuzz_energy_matrix_loader.cpp)
    if (EXISTS ${FUZZ_EMAT_LOADER_FILE})
        set(FUZZ_EMAT_SEEDGEN_FILE ${REPO_ROOT}/src/test/cpp/kstar/gen_fuzz_seeds_energy_matrix_loader.cpp)
        if (EXISTS ${FUZZ_EMAT_SEEDGEN_FILE})
            add_executable(gen_fuzz_seeds_energy_matrix_loader ${FUZZ_EMAT_SEEDGEN_FILE})
            # No libFuzzer linkage here; this is a simple utility binary.
            target_compile_options(gen_fuzz_seeds_energy_matrix_loader PRIVATE -O2)
        endif()

        add_executable(fuzz_energy_matrix_loader ${FUZZ_EMAT_LOADER_FILE})
        target_include_directories(fuzz_energy_matrix_loader PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
        )
        target_link_libraries(fuzz_energy_matrix_loader PRIVATE osprey_kstar_parallel)
        kstar_apply_default_warnings(fuzz_energy_matrix_loader)

        # libFuzzer must only be linked into the fuzzer binary (not libraries).
        target_compile_options(fuzz_energy_matrix_loader PRIVATE -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_energy_matrix_loader PRIVATE -fsanitize=fuzzer,address,undefined)

        if (TARGET gen_fuzz_seeds_energy_matrix_loader)
            add_custom_target(kstar_fuzz_seed_energy_matrix_loader
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/energy_matrix_loader"
                COMMAND ${CMAKE_COMMAND} -E echo "Seeding corpus (if needed)..."
                COMMAND $<TARGET_FILE:gen_fuzz_seeds_energy_matrix_loader> "${CMAKE_BINARY_DIR}/fuzz-corpus/energy_matrix_loader"
                DEPENDS gen_fuzz_seeds_energy_matrix_loader
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generate a small seed corpus for the EnergyMatrixLoader fuzzer"
                VERBATIM
            )
        else()
            add_custom_target(kstar_fuzz_seed_energy_matrix_loader
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/energy_matrix_loader"
                COMMAND ${CMAKE_COMMAND} -E echo "Seed generator not built; corpus directory created but left empty"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Create the corpus directory for the EnergyMatrixLoader fuzzer"
                VERBATIM
            )
        endif()

        add_custom_target(kstar_fuzz_energy_matrix_loader
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-artifacts/energy_matrix_loader"
            COMMAND $<TARGET_FILE:fuzz_energy_matrix_loader>
                -max_total_time=30
                -max_len=65536
                -artifact_prefix=${CMAKE_BINARY_DIR}/fuzz-artifacts/energy_matrix_loader/
                ${CMAKE_BINARY_DIR}/fuzz-corpus/energy_matrix_loader
            DEPENDS fuzz_energy_matrix_loader
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run libFuzzer for EnergyMatrixLoader for 30s; artifacts in ${CMAKE_BINARY_DIR}/fuzz-artifacts/energy_matrix_loader/"
            VERBATIM
        )
    endif()

    # Fuzz target: ConfSearchAStar (baseline/fast) on tiny synthetic EnergyMatrix instances.
    set(FUZZ_CONF_SEARCH_ASTAR_FILE ${REPO_ROOT}/src/test/cpp/kstar/fuzz_conf_search_astar.cpp)
    if (EXISTS ${FUZZ_CONF_SEARCH_ASTAR_FILE})
        set(FUZZ_CONF_SEARCH_ASTAR_SEEDGEN_FILE ${REPO_ROOT}/src/test/cpp/kstar/gen_fuzz_seeds_conf_search_astar.cpp)
        if (EXISTS ${FUZZ_CONF_SEARCH_ASTAR_SEEDGEN_FILE})
            add_executable(gen_fuzz_seeds_conf_search_astar ${FUZZ_CONF_SEARCH_ASTAR_SEEDGEN_FILE})
            # No libFuzzer linkage here; this is a simple utility binary.
            target_compile_options(gen_fuzz_seeds_conf_search_astar PRIVATE -O2)
        endif()

        add_executable(fuzz_conf_search_astar ${FUZZ_CONF_SEARCH_ASTAR_FILE})
        target_include_directories(fuzz_conf_search_astar PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
            ${REPO_ROOT}/src/test/cpp/kstar
        )
        target_link_libraries(fuzz_conf_search_astar PRIVATE osprey_kstar_parallel)
        kstar_apply_default_warnings(fuzz_conf_search_astar)
        kstar_apply_perf_opts(fuzz_conf_search_astar)

        target_compile_options(fuzz_conf_search_astar PRIVATE -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_conf_search_astar PRIVATE -fsanitize=fuzzer,address,undefined)

        add_custom_target(kstar_fuzz_conf_search_astar_corpus_dir
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/conf_search_astar"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Create the corpus directory for the ConfSearchAStar fuzzer"
            VERBATIM
        )

        if (TARGET gen_fuzz_seeds_conf_search_astar)
            add_custom_target(kstar_fuzz_seed_conf_search_astar
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/conf_search_astar"
                COMMAND ${CMAKE_COMMAND} -E echo "Seeding corpus (if needed)..."
                COMMAND $<TARGET_FILE:gen_fuzz_seeds_conf_search_astar> "${CMAKE_BINARY_DIR}/fuzz-corpus/conf_search_astar"
                DEPENDS gen_fuzz_seeds_conf_search_astar
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generate a small seed corpus for the ConfSearchAStar fuzzer"
                VERBATIM
            )
        else()
            add_custom_target(kstar_fuzz_seed_conf_search_astar
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/conf_search_astar"
                COMMAND ${CMAKE_COMMAND} -E echo "Seed generator not built; corpus directory created but left empty"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Create the corpus directory for the ConfSearchAStar fuzzer"
                VERBATIM
            )
        endif()

        add_custom_target(kstar_fuzz_conf_search_astar
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-artifacts/conf_search_astar"
            COMMAND $<TARGET_FILE:fuzz_conf_search_astar>
                -max_total_time=30
                -max_len=2048
                -artifact_prefix=${CMAKE_BINARY_DIR}/fuzz-artifacts/conf_search_astar/
                ${CMAKE_BINARY_DIR}/fuzz-corpus/conf_search_astar
            DEPENDS fuzz_conf_search_astar
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run libFuzzer for ConfSearchAStar for 30s; artifacts in ${CMAKE_BINARY_DIR}/fuzz-artifacts/conf_search_astar/"
            VERBATIM
        )
        add_dependencies(kstar_fuzz_conf_search_astar kstar_fuzz_conf_search_astar_corpus_dir)
        add_dependencies(kstar_fuzz_conf_search_astar kstar_fuzz_seed_conf_search_astar)
    endif()

    # Fuzz target: PartitionFunction on tiny synthetic EnergyMatrix instances.
    set(FUZZ_PARTITION_FUNCTION_FILE ${REPO_ROOT}/src/test/cpp/kstar/fuzz_partition_function.cpp)
    if (EXISTS ${FUZZ_PARTITION_FUNCTION_FILE})
        set(FUZZ_PARTITION_FUNCTION_SEEDGEN_FILE ${REPO_ROOT}/src/test/cpp/kstar/gen_fuzz_seeds_partition_function.cpp)
        if (EXISTS ${FUZZ_PARTITION_FUNCTION_SEEDGEN_FILE})
            add_executable(gen_fuzz_seeds_partition_function ${FUZZ_PARTITION_FUNCTION_SEEDGEN_FILE})
            # No libFuzzer linkage here; this is a simple utility binary.
            target_compile_options(gen_fuzz_seeds_partition_function PRIVATE -O2)
        endif()

        add_executable(fuzz_partition_function ${FUZZ_PARTITION_FUNCTION_FILE})
        target_include_directories(fuzz_partition_function PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
            ${REPO_ROOT}/src/test/cpp/kstar
        )
        target_link_libraries(fuzz_partition_function PRIVATE osprey_kstar_parallel)
        kstar_apply_default_warnings(fuzz_partition_function)
        kstar_apply_perf_opts(fuzz_partition_function)

        target_compile_options(fuzz_partition_function PRIVATE -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined)
        target_link_options(fuzz_partition_function PRIVATE -fsanitize=fuzzer,address,undefined)

        add_custom_target(kstar_fuzz_partition_function_corpus_dir
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/partition_function"
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Create the corpus directory for the PartitionFunction fuzzer"
            VERBATIM
        )

        if (TARGET gen_fuzz_seeds_partition_function)
            add_custom_target(kstar_fuzz_seed_partition_function
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/partition_function"
                COMMAND ${CMAKE_COMMAND} -E echo "Seeding corpus (if needed)..."
                COMMAND $<TARGET_FILE:gen_fuzz_seeds_partition_function> "${CMAKE_BINARY_DIR}/fuzz-corpus/partition_function"
                DEPENDS gen_fuzz_seeds_partition_function
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Generate a small seed corpus for the PartitionFunction fuzzer"
                VERBATIM
            )
        else()
            add_custom_target(kstar_fuzz_seed_partition_function
                COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-corpus/partition_function"
                COMMAND ${CMAKE_COMMAND} -E echo "Seed generator not built; corpus directory created but left empty"
                WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                COMMENT "Create the corpus directory for the PartitionFunction fuzzer"
                VERBATIM
            )
        endif()

        add_custom_target(kstar_fuzz_partition_function
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/fuzz-artifacts/partition_function"
            COMMAND $<TARGET_FILE:fuzz_partition_function>
                -max_total_time=30
                -max_len=2048
                -artifact_prefix=${CMAKE_BINARY_DIR}/fuzz-artifacts/partition_function/
                ${CMAKE_BINARY_DIR}/fuzz-corpus/partition_function
            DEPENDS fuzz_partition_function
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run libFuzzer for PartitionFunction for 30s; artifacts in ${CMAKE_BINARY_DIR}/fuzz-artifacts/partition_function/"
            VERBATIM
        )
        add_dependencies(kstar_fuzz_partition_function kstar_fuzz_partition_function_corpus_dir)
        add_dependencies(kstar_fuzz_partition_function kstar_fuzz_seed_partition_function)
    endif()
endif()

## Test files are at src/test/cpp/kstar/*.cpp relative to repo root
## CMAKE_CURRENT_SOURCE_DIR is src/main/cpp/kstar, so go up 4 levels to repo root
## ../ -> cpp, ../../ -> main, ../../../ -> src, ../../../../ -> repo root
get_filename_component(REPO_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../../../../ ABSOLUTE)

# -----------------------------
# Microbenchmarks (SYNTHESIZED tooling, not OSPREY verbatim tests)
# -----------------------------
if (KSTAR_ENABLE_BENCHMARKS)
    set(KSTAR_PFUNC_BENCH_FILE ${REPO_ROOT}/src/test/cpp/kstar/bench_partition_function_google_benchmark.cpp)
    if (EXISTS ${KSTAR_PFUNC_BENCH_FILE})
        add_executable(kstar_partition_function_bench ${KSTAR_PFUNC_BENCH_FILE})
        target_include_directories(kstar_partition_function_bench PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
            ${REPO_ROOT}/src/test/cpp/kstar
        )
        target_link_libraries(kstar_partition_function_bench PRIVATE osprey_kstar_parallel benchmark::benchmark)
        kstar_apply_default_warnings(kstar_partition_function_bench)
        kstar_apply_perf_opts(kstar_partition_function_bench)

        add_custom_target(kstar_run_partition_function_bench
            COMMAND $<TARGET_FILE:kstar_partition_function_bench> --benchmark_min_time=0.5s
            DEPENDS kstar_partition_function_bench
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run Google Benchmark: partition function (A* baseline/fast + GD)"
            VERBATIM
        )
    endif()

    set(KSTAR_ASTAR_BENCH_FILE ${REPO_ROOT}/src/test/cpp/kstar/bench_astar_search_google_benchmark.cpp)
    if (EXISTS ${KSTAR_ASTAR_BENCH_FILE})
        add_executable(kstar_astar_search_bench ${KSTAR_ASTAR_BENCH_FILE})
        target_include_directories(kstar_astar_search_bench PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
            ${REPO_ROOT}/src/test/cpp/kstar
        )
        target_link_libraries(kstar_astar_search_bench PRIVATE osprey_kstar_parallel benchmark::benchmark)
        kstar_apply_default_warnings(kstar_astar_search_bench)
        kstar_apply_perf_opts(kstar_astar_search_bench)

        add_custom_target(kstar_run_astar_search_bench
            COMMAND $<TARGET_FILE:kstar_astar_search_bench> --benchmark_filter=BM_AStar.* --benchmark_min_time=0.5s
            DEPENDS kstar_astar_search_bench
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run Google Benchmark: A* search hot-loop (baseline vs fast)"
            VERBATIM
        )
    endif()

    set(KSTAR_EXPANDINTO_ROOFLINE_BENCH_FILE ${REPO_ROOT}/src/test/cpp/kstar/bench_expandinto_roofline_google_benchmark.cpp)
    if (EXISTS ${KSTAR_EXPANDINTO_ROOFLINE_BENCH_FILE})
        add_executable(kstar_expandinto_roofline_bench ${KSTAR_EXPANDINTO_ROOFLINE_BENCH_FILE})
        target_include_directories(kstar_expandinto_roofline_bench PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
            ${REPO_ROOT}/src/test/cpp/kstar
        )
        target_link_libraries(kstar_expandinto_roofline_bench PRIVATE osprey_kstar_parallel benchmark::benchmark)
        kstar_apply_default_warnings(kstar_expandinto_roofline_bench)
        kstar_apply_perf_opts(kstar_expandinto_roofline_bench)

        add_custom_target(kstar_run_expandinto_roofline_bench
            COMMAND $<TARGET_FILE:kstar_expandinto_roofline_bench> --benchmark_min_time=0.2s
            DEPENDS kstar_expandinto_roofline_bench
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run Google Benchmark: expandInto roofline microbenchmark (synthetic EnergyMatrix)"
            VERBATIM
        )
    endif()

    set(KSTAR_PFUNC_CHRONO_FILE ${REPO_ROOT}/src/test/cpp/kstar/bench_partition_function_chrono.cpp)
    if (EXISTS ${KSTAR_PFUNC_CHRONO_FILE})
        add_executable(kstar_partition_function_chrono ${KSTAR_PFUNC_CHRONO_FILE})
        target_include_directories(kstar_partition_function_chrono PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${REPO_ROOT}/src/main/cpp/kstar
            ${REPO_ROOT}/src/test/cpp/kstar
        )
        target_link_libraries(kstar_partition_function_chrono PRIVATE osprey_kstar_parallel)
        kstar_apply_default_warnings(kstar_partition_function_chrono)
        kstar_apply_perf_opts(kstar_partition_function_chrono)

        add_custom_target(kstar_run_partition_function_chrono
            COMMAND $<TARGET_FILE:kstar_partition_function_chrono> --reps=3
            DEPENDS kstar_partition_function_chrono
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run chrono benchmark: partition function (prints per-case timing)"
            VERBATIM
        )
    endif()

    # Convenience build target: build only the benchmark executables (no gtests).
    # Usage:
    #   cmake --build <build-dir> --target kstar_bench_build
    add_custom_target(kstar_bench_build)
    if (TARGET kstar_partition_function_bench)
        add_dependencies(kstar_bench_build kstar_partition_function_bench)
    endif()
    if (TARGET kstar_astar_search_bench)
        add_dependencies(kstar_bench_build kstar_astar_search_bench)
    endif()
    if (TARGET kstar_expandinto_roofline_bench)
        add_dependencies(kstar_bench_build kstar_expandinto_roofline_bench)
    endif()
    if (TARGET kstar_partition_function_chrono)
        add_dependencies(kstar_bench_build kstar_partition_function_chrono)
    endif()
endif()

# Partition function tests (GoogleTest)
#
# Keep categories distinct:
# - VERBATIM (Java parity contract)
# - SYNTHESIZED guardrails (baseline vs fast equivalence, etc)
# - SYNTHESIZED precision tiers (oracle-style numeric checks on tiny synthetic spaces)
set(PARTITION_FUNCTION_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_GTEST_FILE})
    add_executable(partition_function_gtest ${PARTITION_FUNCTION_GTEST_FILE})
    target_include_directories(partition_function_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(partition_function_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_gtest)
    kstar_apply_perf_opts(partition_function_gtest)
    kstar_apply_coverage(partition_function_gtest)
    kstar_apply_llvm_coverage(partition_function_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_gtest_tests
    )
    set_tests_properties(${partition_function_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;partition_function"
    )

    message(STATUS "Partition function (VERBATIM) GoogleTest added")
endif()

set(PARTITION_FUNCTION_SYNTHESIZED_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_synthesized_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_SYNTHESIZED_GTEST_FILE})
    add_executable(partition_function_synthesized_gtest ${PARTITION_FUNCTION_SYNTHESIZED_GTEST_FILE})
    target_include_directories(partition_function_synthesized_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(partition_function_synthesized_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_synthesized_gtest)
    kstar_apply_perf_opts(partition_function_synthesized_gtest)
    kstar_apply_coverage(partition_function_synthesized_gtest)
    kstar_apply_llvm_coverage(partition_function_synthesized_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_synthesized_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_synthesized_gtest_tests
    )
    set_tests_properties(${partition_function_synthesized_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;partition_function"
    )
endif()

set(PARTITION_FUNCTION_PRECISION_TIER0_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_precision_tier0_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_PRECISION_TIER0_GTEST_FILE})
    add_executable(partition_function_precision_tier0_gtest ${PARTITION_FUNCTION_PRECISION_TIER0_GTEST_FILE})
    target_include_directories(partition_function_precision_tier0_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(partition_function_precision_tier0_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_precision_tier0_gtest)
    kstar_apply_perf_opts(partition_function_precision_tier0_gtest)
    kstar_apply_coverage(partition_function_precision_tier0_gtest)
    kstar_apply_llvm_coverage(partition_function_precision_tier0_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_precision_tier0_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_precision_tier0_gtest_tests
    )
    set_tests_properties(${partition_function_precision_tier0_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;precision;tier0;partition_function"
    )
endif()

set(PARTITION_FUNCTION_PRECISION_TIER1_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_precision_tier1_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_PRECISION_TIER1_GTEST_FILE})
    add_executable(partition_function_precision_tier1_gtest ${PARTITION_FUNCTION_PRECISION_TIER1_GTEST_FILE})
    target_include_directories(partition_function_precision_tier1_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(partition_function_precision_tier1_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_precision_tier1_gtest)
    kstar_apply_perf_opts(partition_function_precision_tier1_gtest)
    kstar_apply_coverage(partition_function_precision_tier1_gtest)
    kstar_apply_llvm_coverage(partition_function_precision_tier1_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_precision_tier1_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_precision_tier1_gtest_tests
    )
    set_tests_properties(${partition_function_precision_tier1_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;precision;tier1;partition_function"
    )
endif()

# Partition function tutorial/design-choice tests (GoogleTest)
set(PARTITION_FUNCTION_DESIGN_CHOICES_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_design_choices_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_DESIGN_CHOICES_GTEST_FILE})
    add_executable(partition_function_design_choices_gtest ${PARTITION_FUNCTION_DESIGN_CHOICES_GTEST_FILE})
    target_include_directories(partition_function_design_choices_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(partition_function_design_choices_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_design_choices_gtest)
    kstar_apply_perf_opts(partition_function_design_choices_gtest)
    kstar_apply_coverage(partition_function_design_choices_gtest)
    kstar_apply_llvm_coverage(partition_function_design_choices_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_design_choices_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_design_choices_gtest_tests
    )
    set_tests_properties(${partition_function_design_choices_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;tutorial;partition_function"
    )
endif()

# Partition function tutorial tests (GoogleTest)
set(PARTITION_FUNCTION_TUTORIAL_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_pruning_tutorial_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_TUTORIAL_GTEST_FILE})
    add_executable(partition_function_tutorial_gtest ${PARTITION_FUNCTION_TUTORIAL_GTEST_FILE})
    target_include_directories(partition_function_tutorial_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(partition_function_tutorial_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_tutorial_gtest)
    kstar_apply_perf_opts(partition_function_tutorial_gtest)
    kstar_apply_coverage(partition_function_tutorial_gtest)
    kstar_apply_llvm_coverage(partition_function_tutorial_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_tutorial_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_tutorial_gtest_tests
    )
    set_tests_properties(${partition_function_tutorial_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;tutorial;partition_function"
    )
endif()

# Partition function tutorial tests: GradientDescent invariants (GoogleTest)
set(PARTITION_FUNCTION_GD_TUTORIAL_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_gradient_descent_tutorial_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_GD_TUTORIAL_GTEST_FILE})
    add_executable(partition_function_gd_tutorial_gtest ${PARTITION_FUNCTION_GD_TUTORIAL_GTEST_FILE})
    target_include_directories(partition_function_gd_tutorial_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(partition_function_gd_tutorial_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_gd_tutorial_gtest)
    kstar_apply_perf_opts(partition_function_gd_tutorial_gtest)
    kstar_apply_coverage(partition_function_gd_tutorial_gtest)
    kstar_apply_llvm_coverage(partition_function_gd_tutorial_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_gd_tutorial_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_gd_tutorial_gtest_tests
    )
    set_tests_properties(${partition_function_gd_tutorial_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;tutorial;partition_function"
    )
endif()

# Log-space arithmetic tests (GoogleTest) - synthesized
set(LOG_SPACE_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_log_space_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${LOG_SPACE_GTEST_FILE})
    add_executable(log_space_gtest ${LOG_SPACE_GTEST_FILE})
    target_include_directories(log_space_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(log_space_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(log_space_gtest)
    kstar_apply_perf_opts(log_space_gtest)
    kstar_apply_coverage(log_space_gtest)
    kstar_apply_llvm_coverage(log_space_gtest)

    include(GoogleTest)
    gtest_discover_tests(log_space_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST log_space_gtest_tests
    )
    set_tests_properties(${log_space_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;log_space"
    )
endif()

# Log-space arithmetic precision tests (GoogleTest) - synthesized, Tier1
set(LOG_SPACE_PRECISION_TIER1_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_log_space_precision_tier1_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${LOG_SPACE_PRECISION_TIER1_GTEST_FILE})
    add_executable(log_space_precision_tier1_gtest ${LOG_SPACE_PRECISION_TIER1_GTEST_FILE})
    target_include_directories(log_space_precision_tier1_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(log_space_precision_tier1_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(log_space_precision_tier1_gtest)
    kstar_apply_perf_opts(log_space_precision_tier1_gtest)
    kstar_apply_coverage(log_space_precision_tier1_gtest)
    kstar_apply_llvm_coverage(log_space_precision_tier1_gtest)

    include(GoogleTest)
    gtest_discover_tests(log_space_precision_tier1_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST log_space_precision_tier1_gtest_tests
    )
    set_tests_properties(${log_space_precision_tier1_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;precision;tier1;log_space"
    )
endif()

# KStarParallel tests (GoogleTest) - synthesized
set(KSTAR_PARALLEL_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_kstar_parallel.cpp)
if (BUILD_TESTING AND EXISTS ${KSTAR_PARALLEL_GTEST_FILE})
    add_executable(kstar_parallel_gtest ${KSTAR_PARALLEL_GTEST_FILE})
    target_include_directories(kstar_parallel_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(kstar_parallel_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(kstar_parallel_gtest)
    kstar_apply_perf_opts(kstar_parallel_gtest)
    kstar_apply_coverage(kstar_parallel_gtest)
    kstar_apply_llvm_coverage(kstar_parallel_gtest)

    include(GoogleTest)
    gtest_discover_tests(kstar_parallel_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST kstar_parallel_gtest_tests
    )
    set_tests_properties(${kstar_parallel_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized"
    )
endif()

# EnergyMatrix tests (GoogleTest) - synthesized
set(ENERGY_MATRIX_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_energy_matrix.cpp)
if (BUILD_TESTING AND EXISTS ${ENERGY_MATRIX_GTEST_FILE})
    add_executable(energy_matrix_gtest ${ENERGY_MATRIX_GTEST_FILE})
    target_include_directories(energy_matrix_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(energy_matrix_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(energy_matrix_gtest)
    kstar_apply_perf_opts(energy_matrix_gtest)
    kstar_apply_coverage(energy_matrix_gtest)
    kstar_apply_llvm_coverage(energy_matrix_gtest)

    include(GoogleTest)
    gtest_discover_tests(energy_matrix_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST energy_matrix_gtest_tests
    )
    set_tests_properties(${energy_matrix_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;energy_matrix"
    )
endif()

# EnergyMatrix OSPREY pattern tests (GoogleTest) - synthesized
set(ENERGY_MATRIX_OSPREY_PATTERN_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_energy_matrix_osprey_pattern.cpp)
if (BUILD_TESTING AND EXISTS ${ENERGY_MATRIX_OSPREY_PATTERN_GTEST_FILE})
    add_executable(energy_matrix_osprey_pattern_gtest ${ENERGY_MATRIX_OSPREY_PATTERN_GTEST_FILE})
    target_include_directories(energy_matrix_osprey_pattern_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(energy_matrix_osprey_pattern_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(energy_matrix_osprey_pattern_gtest)
    kstar_apply_perf_opts(energy_matrix_osprey_pattern_gtest)
    kstar_apply_coverage(energy_matrix_osprey_pattern_gtest)
    kstar_apply_llvm_coverage(energy_matrix_osprey_pattern_gtest)

    include(GoogleTest)
    gtest_discover_tests(energy_matrix_osprey_pattern_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST energy_matrix_osprey_pattern_gtest_tests
    )
    set_tests_properties(${energy_matrix_osprey_pattern_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;energy_matrix"
    )
endif()

# EnergyMatrix Java comparison tests (GoogleTest)
set(ENERGY_MATRIX_JAVA_COMPARISON_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_energy_matrix_java_comparison.cpp)
if (BUILD_TESTING AND EXISTS ${ENERGY_MATRIX_JAVA_COMPARISON_GTEST_FILE})
    add_executable(energy_matrix_java_comparison_gtest ${ENERGY_MATRIX_JAVA_COMPARISON_GTEST_FILE})
    target_include_directories(energy_matrix_java_comparison_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(energy_matrix_java_comparison_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(energy_matrix_java_comparison_gtest)
    kstar_apply_perf_opts(energy_matrix_java_comparison_gtest)
    kstar_apply_coverage(energy_matrix_java_comparison_gtest)
    kstar_apply_llvm_coverage(energy_matrix_java_comparison_gtest)

    include(GoogleTest)
    gtest_discover_tests(energy_matrix_java_comparison_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST energy_matrix_java_comparison_gtest_tests
    )
    set_tests_properties(${energy_matrix_java_comparison_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;energy_matrix"
    )
endif()

# A* search tests (GoogleTest) - synthesized
set(ASTAR_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_astar_search.cpp)
if (BUILD_TESTING AND EXISTS ${ASTAR_GTEST_FILE})
    add_executable(astar_gtest ${ASTAR_GTEST_FILE})
    target_include_directories(astar_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(astar_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(astar_gtest)
    kstar_apply_perf_opts(astar_gtest)
    kstar_apply_coverage(astar_gtest)
    kstar_apply_llvm_coverage(astar_gtest)

    include(GoogleTest)
    gtest_discover_tests(astar_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST astar_gtest_tests
    )
    set_tests_properties(${astar_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;astar"
    )
endif()

# A* node OSPREY tests (GoogleTest) - VERBATIM
set(ASTAR_NODE_OSPREY_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_astar_node_osprey.cpp)
if (BUILD_TESTING AND EXISTS ${ASTAR_NODE_OSPREY_GTEST_FILE})
    add_executable(astar_node_osprey_gtest ${ASTAR_NODE_OSPREY_GTEST_FILE})
    target_include_directories(astar_node_osprey_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(astar_node_osprey_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(astar_node_osprey_gtest)
    kstar_apply_perf_opts(astar_node_osprey_gtest)
    kstar_apply_coverage(astar_node_osprey_gtest)
    kstar_apply_llvm_coverage(astar_node_osprey_gtest)

    include(GoogleTest)
    gtest_discover_tests(astar_node_osprey_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST astar_node_osprey_gtest_tests
    )
    set_tests_properties(${astar_node_osprey_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;astar_node"
    )
endif()

# ConfIndex tests (GoogleTest)
set(CONF_INDEX_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_conf_index_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${CONF_INDEX_GTEST_FILE})
    add_executable(conf_index_gtest ${CONF_INDEX_GTEST_FILE})
    target_include_directories(conf_index_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(conf_index_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(conf_index_gtest)
    kstar_apply_perf_opts(conf_index_gtest)
    kstar_apply_coverage(conf_index_gtest)
    kstar_apply_llvm_coverage(conf_index_gtest)

    include(GoogleTest)
    gtest_discover_tests(conf_index_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST conf_index_gtest_tests
    )
    set_tests_properties(${conf_index_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;conf_index"
    )

    message(STATUS "ConfIndex GoogleTest added")
endif()

# ConfRanker tests (GoogleTest)
set(CONF_RANKER_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_conf_ranker_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${CONF_RANKER_GTEST_FILE})
    add_executable(conf_ranker_gtest ${CONF_RANKER_GTEST_FILE})
    target_include_directories(conf_ranker_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(conf_ranker_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(conf_ranker_gtest)
    kstar_apply_perf_opts(conf_ranker_gtest)
    kstar_apply_coverage(conf_ranker_gtest)
    kstar_apply_llvm_coverage(conf_ranker_gtest)

    include(GoogleTest)
    gtest_discover_tests(conf_ranker_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST conf_ranker_gtest_tests
    )
    set_tests_properties(${conf_ranker_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;conf_ranker"
    )

    message(STATUS "ConfRanker GoogleTest added")
endif()

# ConfSearchCache tests (GoogleTest)
set(CONF_SEARCH_CACHE_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_conf_search_cache_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${CONF_SEARCH_CACHE_GTEST_FILE})
    add_executable(conf_search_cache_gtest ${CONF_SEARCH_CACHE_GTEST_FILE})
    target_include_directories(conf_search_cache_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(conf_search_cache_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(conf_search_cache_gtest)
    kstar_apply_perf_opts(conf_search_cache_gtest)
    kstar_apply_coverage(conf_search_cache_gtest)
    kstar_apply_llvm_coverage(conf_search_cache_gtest)

    include(GoogleTest)
    gtest_discover_tests(conf_search_cache_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST conf_search_cache_gtest_tests
    )
    set_tests_properties(${conf_search_cache_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;conf_search_cache"
    )

    message(STATUS "ConfSearchCache GoogleTest added")
endif()

# ConfSearch (A*) tests (GoogleTest) - synthesized
set(CONF_SEARCH_ASTAR_SYNTH_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_conf_search_astar_synthesized_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${CONF_SEARCH_ASTAR_SYNTH_GTEST_FILE})
    add_executable(conf_search_astar_synthesized_gtest ${CONF_SEARCH_ASTAR_SYNTH_GTEST_FILE})
    target_include_directories(conf_search_astar_synthesized_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(conf_search_astar_synthesized_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(conf_search_astar_synthesized_gtest)
    kstar_apply_perf_opts(conf_search_astar_synthesized_gtest)
    kstar_apply_coverage(conf_search_astar_synthesized_gtest)
    kstar_apply_llvm_coverage(conf_search_astar_synthesized_gtest)

    include(GoogleTest)
    gtest_discover_tests(conf_search_astar_synthesized_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST conf_search_astar_synthesized_gtest_tests
    )
    set_tests_properties(${conf_search_astar_synthesized_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;conf_search"
    )
endif()

# KStarScore tests (GoogleTest) - VERBATIM
set(KSTAR_SCORE_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_kstar_score_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${KSTAR_SCORE_GTEST_FILE})
    add_executable(kstar_score_gtest ${KSTAR_SCORE_GTEST_FILE})
    target_include_directories(kstar_score_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(kstar_score_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(kstar_score_gtest)
    kstar_apply_perf_opts(kstar_score_gtest)
    kstar_apply_coverage(kstar_score_gtest)
    kstar_apply_llvm_coverage(kstar_score_gtest)

    include(GoogleTest)
    gtest_discover_tests(kstar_score_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST kstar_score_gtest_tests
    )
    set_tests_properties(${kstar_score_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;verbatim;kstar_score"
    )
endif()

# K* workflow tests (GoogleTest) - synthesized
set(KSTAR_WORKFLOW_SYNTH_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_kstar_workflow_synthesized_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${KSTAR_WORKFLOW_SYNTH_GTEST_FILE})
    add_executable(kstar_workflow_synthesized_gtest ${KSTAR_WORKFLOW_SYNTH_GTEST_FILE})
    target_include_directories(kstar_workflow_synthesized_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(kstar_workflow_synthesized_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(kstar_workflow_synthesized_gtest)
    kstar_apply_perf_opts(kstar_workflow_synthesized_gtest)
    kstar_apply_coverage(kstar_workflow_synthesized_gtest)
    kstar_apply_llvm_coverage(kstar_workflow_synthesized_gtest)

    include(GoogleTest)
    gtest_discover_tests(kstar_workflow_synthesized_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST kstar_workflow_synthesized_gtest_tests
    )
    set_tests_properties(${kstar_workflow_synthesized_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;kstar_workflow"
    )
endif()

# -----------------------------
# CTest "meta" targets for hierarchical runs
# (works even if per-test discovery labels aren't available)
# -----------------------------
if (BUILD_TESTING)

    # Run each gtest binary as a single CTest test, with labels for hierarchy.
    if (TARGET partition_function_gtest)
        add_test(NAME kstar.partition_function COMMAND $<TARGET_FILE:partition_function_gtest>)
        set_tests_properties(kstar.partition_function PROPERTIES LABELS "kstar;gtest;verbatim;partition_function")
    endif()
    if (TARGET partition_function_synthesized_gtest)
        add_test(NAME kstar.partition_function_synthesized COMMAND $<TARGET_FILE:partition_function_synthesized_gtest>)
        set_tests_properties(kstar.partition_function_synthesized PROPERTIES LABELS "kstar;gtest;synthesized;partition_function")
    endif()
    if (TARGET partition_function_precision_tier0_gtest)
        add_test(NAME kstar.partition_function_precision_tier0 COMMAND $<TARGET_FILE:partition_function_precision_tier0_gtest>)
        set_tests_properties(kstar.partition_function_precision_tier0 PROPERTIES LABELS "kstar;gtest;synthesized;precision;tier0;partition_function")
    endif()
    if (TARGET partition_function_precision_tier1_gtest)
        add_test(NAME kstar.partition_function_precision_tier1 COMMAND $<TARGET_FILE:partition_function_precision_tier1_gtest>)
        set_tests_properties(kstar.partition_function_precision_tier1 PROPERTIES LABELS "kstar;gtest;synthesized;precision;tier1;partition_function")
    endif()

    if (TARGET log_space_gtest)
        add_test(NAME kstar.log_space COMMAND $<TARGET_FILE:log_space_gtest>)
        set_tests_properties(kstar.log_space PROPERTIES LABELS "kstar;gtest;synthesized;log_space")
    endif()
    if (TARGET log_space_precision_tier1_gtest)
        add_test(NAME kstar.log_space_precision_tier1 COMMAND $<TARGET_FILE:log_space_precision_tier1_gtest>)
        set_tests_properties(kstar.log_space_precision_tier1 PROPERTIES LABELS "kstar;gtest;synthesized;precision;tier1;log_space")
    endif()

    if (TARGET kstar_workflow_synthesized_gtest)
        add_test(NAME kstar.kstar_workflow COMMAND $<TARGET_FILE:kstar_workflow_synthesized_gtest>)
        set_tests_properties(kstar.kstar_workflow PROPERTIES LABELS "kstar;gtest;synthesized;kstar_workflow")
    endif()

    # Optional: benchmark smoke tests (very short runs) so `ctest -R kstar\\.bench_` works.
    if (KSTAR_ENABLE_BENCHMARKS AND TARGET kstar_astar_search_bench)
        add_test(NAME kstar.bench_astar_smoke COMMAND $<TARGET_FILE:kstar_astar_search_bench>
            # Smoke should not depend on external env vars (e.g., case:5 "External").
            # Avoid parentheses here: CMake/CTest can split args in surprising ways.
            --benchmark_filter=^BM_AStar.*_PopExpand/case:[0-4]/.*
            --benchmark_min_time=0.01s
        )
        set_tests_properties(kstar.bench_astar_smoke PROPERTIES LABELS "kstar;benchmark;synthesized;smoke;astar")
    endif()
    if (KSTAR_ENABLE_BENCHMARKS AND TARGET kstar_partition_function_bench)
        add_test(NAME kstar.bench_pfunc_smoke COMMAND $<TARGET_FILE:kstar_partition_function_bench>
            --benchmark_filter=BM_Pfunc_.*
            --benchmark_min_time=0.01s
        )
        set_tests_properties(kstar.bench_pfunc_smoke PROPERTIES LABELS "kstar;benchmark;synthesized;smoke;partition_function")
    endif()

    # Benchmark category: memory hierarchy pressure (longer, larger working sets).
    # Intended for explicit runs: `ctest -R '^kstar\\.bench_mem\\.'` or `ctest -L mem_hierarchy`.
    #
    # IMPORTANT: avoid parentheses/alternation in --benchmark_filter here. CTest can split args
    # in surprising ways, and Google Benchmark will then interpret stray tokens as flags.
    if (KSTAR_ENABLE_BENCHMARKS AND TARGET kstar_expandinto_roofline_bench)
        add_test(NAME kstar.bench_mem.expandinto_roofline_32 COMMAND $<TARGET_FILE:kstar_expandinto_roofline_bench>
            --benchmark_filter=^BM_AStarFast_ExpandInto_Roofline/num_pos:32/rc:64/.*
            --benchmark_min_time=0.2s
            --benchmark_repetitions=3
            --benchmark_report_aggregates_only=true
        )
        set_tests_properties(kstar.bench_mem.expandinto_roofline_32 PROPERTIES LABELS "kstar;benchmark;synthesized;mem_hierarchy;expandInto;roofline")

        add_test(NAME kstar.bench_mem.expandinto_roofline_48 COMMAND $<TARGET_FILE:kstar_expandinto_roofline_bench>
            --benchmark_filter=^BM_AStarFast_ExpandInto_Roofline/num_pos:48/rc:64/.*
            --benchmark_min_time=0.2s
            --benchmark_repetitions=3
            --benchmark_report_aggregates_only=true
        )
        set_tests_properties(kstar.bench_mem.expandinto_roofline_48 PROPERTIES LABELS "kstar;benchmark;synthesized;mem_hierarchy;expandInto;roofline")

        # Explicit DRAM-scale point (larger pairwise block working set).
        add_test(NAME kstar.bench_mem.expandinto_roofline_64 COMMAND $<TARGET_FILE:kstar_expandinto_roofline_bench>
            --benchmark_filter=^BM_AStarFast_ExpandInto_Roofline/num_pos:64/rc:64/.*
            --benchmark_min_time=0.2s
            --benchmark_repetitions=3
            --benchmark_report_aggregates_only=true
        )
        set_tests_properties(kstar.bench_mem.expandinto_roofline_64 PROPERTIES LABELS "kstar;benchmark;synthesized;mem_hierarchy;expandInto;roofline;dram")

        # Very large DRAM-scale point (setup cost is higher; keep repetitions low).
        add_test(NAME kstar.bench_mem.expandinto_roofline_96 COMMAND $<TARGET_FILE:kstar_expandinto_roofline_bench>
            --benchmark_filter=^BM_AStarFast_ExpandInto_Roofline/num_pos:96/rc:64/.*
            --benchmark_min_time=0.2s
            --benchmark_repetitions=1
            --benchmark_report_aggregates_only=true
        )
        set_tests_properties(kstar.bench_mem.expandinto_roofline_96 PROPERTIES LABELS "kstar;benchmark;synthesized;mem_hierarchy;expandInto;roofline;dram")
    endif()

    # Convenience umbrella for the precision suite (single entrypoint).
    if (TARGET partition_function_precision_tier0_gtest AND TARGET partition_function_precision_tier1_gtest AND TARGET log_space_precision_tier1_gtest)
        # Convenience build target: only the binaries executed by kstar.precision_all.
        # Usage:
        #   cmake --build <build-dir> --target kstar_precision_build
        add_custom_target(kstar_precision_build
            DEPENDS
                partition_function_precision_tier0_gtest
                partition_function_precision_tier1_gtest
                log_space_precision_tier1_gtest
        )

        add_test(NAME kstar.precision_all COMMAND /bin/bash -lc
            "$<TARGET_FILE:partition_function_precision_tier0_gtest> && $<TARGET_FILE:partition_function_precision_tier1_gtest> && $<TARGET_FILE:log_space_precision_tier1_gtest>"
        )
        set_tests_properties(kstar.precision_all PROPERTIES LABELS "kstar;gtest;synthesized;precision")
    endif()

    # Sanitizer smoke suite (intended for a dedicated ASan/UBSan build tree).
    if (KSTAR_ENABLE_SANITIZERS AND TARGET partition_function_precision_tier0_gtest AND TARGET partition_function_precision_tier1_gtest AND TARGET log_space_precision_tier1_gtest)
        # Convenience build target: only the binaries executed by kstar.sanitizers_smoke.
        # Usage:
        #   cmake --build <build-dir> --target kstar_sanitizers_smoke_build
        add_custom_target(kstar_sanitizers_smoke_build
            DEPENDS
                partition_function_precision_tier0_gtest
                partition_function_precision_tier1_gtest
                log_space_precision_tier1_gtest
        )

        add_test(NAME kstar.sanitizers_smoke COMMAND /bin/bash -lc
            "ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $<TARGET_FILE:partition_function_precision_tier0_gtest> && ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $<TARGET_FILE:partition_function_precision_tier1_gtest> && ASAN_OPTIONS=abort_on_error=1:halt_on_error=1:detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $<TARGET_FILE:log_space_precision_tier1_gtest>"
        )
        set_tests_properties(kstar.sanitizers_smoke PROPERTIES LABELS "kstar;gtest;synthesized;sanitizers")
    endif()

    # Static analysis (clang-tidy) - SYNTHESIZED category for bug-finding.
    #
    # This is intentionally a curated subset to keep runtime and noise stable.
    set(_KSTAR_CLANG_TIDY_FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/partition_function.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/energy_matrix_loader.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/energy_matrix.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/astar_search.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/astar_search_fast.cpp
    )

    add_custom_target(kstar_clang_tidy
        COMMAND /bin/bash -lc
            "command -v clang-tidy >/dev/null 2>&1 || { echo 'ERROR: clang-tidy not found. Install: sudo apt-get install -y clang-tidy'; exit 1; } ; \
             test -f \"${CMAKE_BINARY_DIR}/compile_commands.json\" || { echo 'ERROR: compile_commands.json not found in build dir'; exit 1; } ; \
             clang-tidy -p \"${CMAKE_BINARY_DIR}\" ${_KSTAR_CLANG_TIDY_FILES}"
        VERBATIM
    )

    add_test(NAME kstar.clang_tidy COMMAND /bin/bash -lc
        "command -v clang-tidy >/dev/null 2>&1 || { echo 'SKIP: clang-tidy not found. Install: sudo apt-get install -y clang-tidy'; exit 125; } ; \
         cmake --build \"${CMAKE_BINARY_DIR}\" --target kstar_clang_tidy"
    )
    set_tests_properties(kstar.clang_tidy PROPERTIES
        LABELS "kstar;static_analysis;clang_tidy;synthesized"
        SKIP_RETURN_CODE 125
    )

    # Tier 2: CBMC (bounded model checking) on the smallest surfaces.
    # This is intentionally optional and skips cleanly when cbmc is not installed.
    set(KSTAR_CBMC_LOG_SPACE_HARNESS ${REPO_ROOT}/src/test/cpp/kstar/cbmc_log_space_tier2.cpp)
    if (EXISTS ${KSTAR_CBMC_LOG_SPACE_HARNESS})
        add_custom_target(kstar_cbmc_log_space_tier2
            COMMAND /bin/bash -lc
                "command -v cbmc >/dev/null 2>&1 || { echo 'ERROR: cbmc not found. Install: sudo apt-get install -y cbmc'; exit 1; } ; \
                 cbmc --version ; \
                 cbmc --stop-on-fail --unwind 1 --bounds-check --pointer-check --div-by-zero-check --signed-overflow-check --nan-check --float-overflow-check \
                      --cpp11 \
                      -I\"${REPO_ROOT}/src/main/cpp/kstar\" \
                      \"${KSTAR_CBMC_LOG_SPACE_HARNESS}\""
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run CBMC Tier2: log_space.hpp early-return/cutoff proofs"
            VERBATIM
        )

        add_test(NAME kstar.cbmc_log_space_tier2 COMMAND /bin/bash -lc
            "command -v cbmc >/dev/null 2>&1 || { echo 'SKIP: cbmc not found. Install: sudo apt-get install -y cbmc'; exit 125; } ; \
             cmake --build \"${CMAKE_BINARY_DIR}\" --target kstar_cbmc_log_space_tier2"
        )
        set_tests_properties(kstar.cbmc_log_space_tier2 PROPERTIES
            LABELS "kstar;cbmc;synthesized;precision;tier2;log_space"
            SKIP_RETURN_CODE 125
        )
    endif()

    set(KSTAR_CBMC_PFUNC_HARNESS ${REPO_ROOT}/src/test/cpp/kstar/cbmc_partition_function_tier2.cpp)
    if (EXISTS ${KSTAR_CBMC_PFUNC_HARNESS})
        add_custom_target(kstar_cbmc_partition_function_tier2
            COMMAND /bin/bash -lc
                "command -v cbmc >/dev/null 2>&1 || { echo 'ERROR: cbmc not found. Install: sudo apt-get install -y cbmc'; exit 1; } ; \
                 cbmc --version ; \
                 cbmc --stop-on-fail --unwind 1 --bounds-check --pointer-check --div-by-zero-check --signed-overflow-check --nan-check --float-overflow-check \
                      --cpp11 \
                      \"${KSTAR_CBMC_PFUNC_HARNESS}\""
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run CBMC Tier2: PartitionFunction delta/cutoff invariants"
            VERBATIM
        )

        add_test(NAME kstar.cbmc_partition_function_tier2 COMMAND /bin/bash -lc
            "command -v cbmc >/dev/null 2>&1 || { echo 'SKIP: cbmc not found. Install: sudo apt-get install -y cbmc'; exit 125; } ; \
             cmake --build \"${CMAKE_BINARY_DIR}\" --target kstar_cbmc_partition_function_tier2"
        )
        set_tests_properties(kstar.cbmc_partition_function_tier2 PROPERTIES
            LABELS "kstar;cbmc;synthesized;precision;tier2;partition_function"
            SKIP_RETURN_CODE 125
        )
    endif()

    # Convenience umbrella for Tier2 CBMC checks (single entrypoint).
    if (EXISTS ${KSTAR_CBMC_LOG_SPACE_HARNESS} AND EXISTS ${KSTAR_CBMC_PFUNC_HARNESS})
        add_custom_target(kstar_cbmc_all_tier2_build
            DEPENDS
                kstar_cbmc_log_space_tier2
                kstar_cbmc_partition_function_tier2
        )

        add_test(NAME kstar.cbmc_all COMMAND /bin/bash -lc
            "command -v cbmc >/dev/null 2>&1 || { echo 'SKIP: cbmc not found. Install: sudo apt-get install -y cbmc'; exit 125; } ; \
             cmake --build \"${CMAKE_BINARY_DIR}\" --target kstar_cbmc_all_tier2_build"
        )
        set_tests_properties(kstar.cbmc_all PROPERTIES
            LABELS "kstar;cbmc;synthesized;precision;tier2"
            SKIP_RETURN_CODE 125
        )
    endif()

    # Compile-fail tests (C++20 constraints and [[nodiscard]] enforcement)
    set(KSTAR_COMPILE_FAIL_SCRIPT ${REPO_ROOT}/src/test/cpp/kstar/compile_fail/compile_fail.cmake)
    if (EXISTS ${KSTAR_COMPILE_FAIL_SCRIPT})
        add_test(NAME kstar.compile_fail COMMAND /bin/bash -lc
            "${CMAKE_COMMAND} -DREPO_ROOT='${REPO_ROOT}' -DBUILD_DIR='${CMAKE_BINARY_DIR}' -DCXX_COMPILER='${CMAKE_CXX_COMPILER}' -P '${KSTAR_COMPILE_FAIL_SCRIPT}'"
        )
        set_tests_properties(kstar.compile_fail PROPERTIES LABELS "kstar;compile_fail;cxx20")
    endif()

    if (TARGET conf_index_gtest)
        add_test(NAME kstar.conf_index COMMAND $<TARGET_FILE:conf_index_gtest>)
        set_tests_properties(kstar.conf_index PROPERTIES LABELS "kstar;gtest;verbatim;conf_index")
    endif()

    if (TARGET conf_ranker_gtest)
        add_test(NAME kstar.conf_ranker COMMAND $<TARGET_FILE:conf_ranker_gtest>)
        set_tests_properties(kstar.conf_ranker PROPERTIES LABELS "kstar;gtest;verbatim;conf_ranker")
    endif()

    if (TARGET conf_search_cache_gtest)
        add_test(NAME kstar.conf_search_cache COMMAND $<TARGET_FILE:conf_search_cache_gtest>)
        set_tests_properties(kstar.conf_search_cache PROPERTIES LABELS "kstar;gtest;verbatim;conf_search_cache")
    endif()

    if (TARGET kstar_score_gtest)
        add_test(NAME kstar.kstar_score COMMAND $<TARGET_FILE:kstar_score_gtest>)
        set_tests_properties(kstar.kstar_score PROPERTIES LABELS "kstar;gtest;verbatim;kstar_score")
    endif()

    # Convenience umbrella target for "all currently-ported VERBATIM gtests"
    # (kept simple to avoid re-entering ctest from within ctest)
    if (TARGET partition_function_gtest AND TARGET conf_index_gtest AND TARGET conf_ranker_gtest AND TARGET conf_search_cache_gtest AND TARGET kstar_score_gtest)
        add_test(NAME kstar.verbatim_all COMMAND /bin/bash -lc
            "$<TARGET_FILE:partition_function_gtest> && $<TARGET_FILE:conf_index_gtest> && $<TARGET_FILE:conf_ranker_gtest> && $<TARGET_FILE:conf_search_cache_gtest> && $<TARGET_FILE:kstar_score_gtest>"
        )
        set_tests_properties(kstar.verbatim_all PROPERTIES LABELS "kstar;gtest;verbatim")
    endif()

endif()

# ConfSpace integration tests (placeholder - requires JNA/Python bridge)
set(CONFSPACE_TEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_confspace_integration.cpp)
if(EXISTS ${CONFSPACE_TEST_FILE})
    add_executable(confspace_test ${CONFSPACE_TEST_FILE})
    target_include_directories(confspace_test PRIVATE 
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/main/cc/ConfEcalc
    )
    target_link_libraries(confspace_test osprey_kstar_parallel)
    message(STATUS "ConfSpace integration test added (placeholder - requires JNA/Python bridge)")
endif()

# EnergyMatrixLoader corpus runner (replays fuzz corpus to drive coverage)
set(ENERGY_MATRIX_LOADER_CORPUS_RUNNER_FILE ${REPO_ROOT}/src/test/cpp/kstar/energy_matrix_loader_corpus_runner.cpp)
if (BUILD_TESTING AND EXISTS ${ENERGY_MATRIX_LOADER_CORPUS_RUNNER_FILE})
    add_executable(energy_matrix_loader_corpus_runner ${ENERGY_MATRIX_LOADER_CORPUS_RUNNER_FILE})
    target_include_directories(energy_matrix_loader_corpus_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
    )
    target_link_libraries(energy_matrix_loader_corpus_runner PRIVATE osprey_kstar_parallel)
    target_compile_definitions(energy_matrix_loader_corpus_runner PRIVATE OSPREY_REPO_ROOT="${REPO_ROOT}")
    kstar_apply_default_warnings(energy_matrix_loader_corpus_runner)
    kstar_apply_perf_opts(energy_matrix_loader_corpus_runner)
    kstar_apply_coverage(energy_matrix_loader_corpus_runner)
    kstar_apply_llvm_coverage(energy_matrix_loader_corpus_runner)

    add_test(NAME kstar.energy_matrix_loader_corpus_runner COMMAND $<TARGET_FILE:energy_matrix_loader_corpus_runner>)
    set_tests_properties(kstar.energy_matrix_loader_corpus_runner PROPERTIES LABELS "kstar;coverage;fuzz_corpus")
endif()

set(CONF_SEARCH_ASTAR_CORPUS_RUNNER_FILE ${REPO_ROOT}/src/test/cpp/kstar/conf_search_astar_corpus_runner.cpp)
if (BUILD_TESTING AND EXISTS ${CONF_SEARCH_ASTAR_CORPUS_RUNNER_FILE})
    add_executable(conf_search_astar_corpus_runner ${CONF_SEARCH_ASTAR_CORPUS_RUNNER_FILE})
    target_include_directories(conf_search_astar_corpus_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(conf_search_astar_corpus_runner PRIVATE osprey_kstar_parallel)
    target_compile_definitions(conf_search_astar_corpus_runner PRIVATE OSPREY_REPO_ROOT="${REPO_ROOT}")
    kstar_apply_default_warnings(conf_search_astar_corpus_runner)
    kstar_apply_perf_opts(conf_search_astar_corpus_runner)
    kstar_apply_coverage(conf_search_astar_corpus_runner)
    kstar_apply_llvm_coverage(conf_search_astar_corpus_runner)

    add_test(NAME kstar.conf_search_astar_corpus_runner COMMAND $<TARGET_FILE:conf_search_astar_corpus_runner>)
    set_tests_properties(kstar.conf_search_astar_corpus_runner PROPERTIES LABELS "kstar;coverage;fuzz_corpus")
endif()

set(PARTITION_FUNCTION_CORPUS_RUNNER_FILE ${REPO_ROOT}/src/test/cpp/kstar/partition_function_corpus_runner.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_CORPUS_RUNNER_FILE})
    add_executable(partition_function_corpus_runner ${PARTITION_FUNCTION_CORPUS_RUNNER_FILE})
    target_include_directories(partition_function_corpus_runner PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(partition_function_corpus_runner PRIVATE osprey_kstar_parallel)
    target_compile_definitions(partition_function_corpus_runner PRIVATE OSPREY_REPO_ROOT="${REPO_ROOT}")
    kstar_apply_default_warnings(partition_function_corpus_runner)
    kstar_apply_perf_opts(partition_function_corpus_runner)
    kstar_apply_coverage(partition_function_corpus_runner)
    kstar_apply_llvm_coverage(partition_function_corpus_runner)

    add_test(NAME kstar.partition_function_corpus_runner COMMAND $<TARGET_FILE:partition_function_corpus_runner>)
    set_tests_properties(kstar.partition_function_corpus_runner PROPERTIES LABELS "kstar;coverage;fuzz_corpus")
endif()

# EnergyMatrixLoader regression tests (promoted fuzz artifacts, repo-local)
set(ENERGY_MATRIX_LOADER_REGRESSION_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_energy_matrix_loader_regression_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${ENERGY_MATRIX_LOADER_REGRESSION_GTEST_FILE})
    add_executable(energy_matrix_loader_regression_gtest ${ENERGY_MATRIX_LOADER_REGRESSION_GTEST_FILE})
    target_include_directories(energy_matrix_loader_regression_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(energy_matrix_loader_regression_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(energy_matrix_loader_regression_gtest)
    kstar_apply_perf_opts(energy_matrix_loader_regression_gtest)
    kstar_apply_coverage(energy_matrix_loader_regression_gtest)
    kstar_apply_llvm_coverage(energy_matrix_loader_regression_gtest)

    include(GoogleTest)
    gtest_discover_tests(energy_matrix_loader_regression_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST energy_matrix_loader_regression_gtest_tests
    )
    set_tests_properties(${energy_matrix_loader_regression_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;coverage_regression;energy_matrix_loader"
    )

    add_test(NAME kstar.energy_matrix_loader_regression COMMAND $<TARGET_FILE:energy_matrix_loader_regression_gtest>)
    set_tests_properties(kstar.energy_matrix_loader_regression PROPERTIES LABELS "kstar;gtest;synthesized;coverage_regression;energy_matrix_loader")
endif()

# PartitionFunction regression tests (promoted fuzz artifacts, repo-local)
set(PARTITION_FUNCTION_REGRESSION_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_partition_function_regression_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${PARTITION_FUNCTION_REGRESSION_GTEST_FILE})
    add_executable(partition_function_regression_gtest ${PARTITION_FUNCTION_REGRESSION_GTEST_FILE})
    target_include_directories(partition_function_regression_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(partition_function_regression_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(partition_function_regression_gtest)
    kstar_apply_perf_opts(partition_function_regression_gtest)
    kstar_apply_coverage(partition_function_regression_gtest)
    kstar_apply_llvm_coverage(partition_function_regression_gtest)

    include(GoogleTest)
    gtest_discover_tests(partition_function_regression_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST partition_function_regression_gtest_tests
    )
    set_tests_properties(${partition_function_regression_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;coverage_regression;partition_function"
    )

    add_test(NAME kstar.partition_function_regression COMMAND $<TARGET_FILE:partition_function_regression_gtest>)
    set_tests_properties(kstar.partition_function_regression PROPERTIES LABELS "kstar;gtest;synthesized;coverage_regression;partition_function")
endif()

# ConfSearchAStar regression tests (promoted fuzz artifacts, repo-local)
set(CONF_SEARCH_ASTAR_REGRESSION_GTEST_FILE ${REPO_ROOT}/src/test/cpp/kstar/test_conf_search_astar_regression_gtest.cpp)
if (BUILD_TESTING AND EXISTS ${CONF_SEARCH_ASTAR_REGRESSION_GTEST_FILE})
    add_executable(conf_search_astar_regression_gtest ${CONF_SEARCH_ASTAR_REGRESSION_GTEST_FILE})
    target_include_directories(conf_search_astar_regression_gtest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${REPO_ROOT}/src/main/cpp/kstar
        ${REPO_ROOT}/src/test/cpp/kstar
    )
    target_link_libraries(conf_search_astar_regression_gtest PRIVATE osprey_kstar_parallel GTest::gtest GTest::gtest_main)
    kstar_apply_default_warnings(conf_search_astar_regression_gtest)
    kstar_apply_perf_opts(conf_search_astar_regression_gtest)
    kstar_apply_coverage(conf_search_astar_regression_gtest)
    kstar_apply_llvm_coverage(conf_search_astar_regression_gtest)

    include(GoogleTest)
    gtest_discover_tests(conf_search_astar_regression_gtest
        DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
        TEST_LIST conf_search_astar_regression_gtest_tests
    )
    set_tests_properties(${conf_search_astar_regression_gtest_tests} PROPERTIES
        LABELS "kstar;gtest;synthesized;coverage_regression;conf_search_astar"
    )

    add_test(NAME kstar.conf_search_astar_regression COMMAND $<TARGET_FILE:conf_search_astar_regression_gtest>)
    set_tests_properties(kstar.conf_search_astar_regression PROPERTIES LABELS "kstar;gtest;synthesized;coverage_regression;conf_search_astar")
endif()

# -----------------------------
# "Full check" convenience target: build all tests + run ctest
# -----------------------------
if (BUILD_TESTING)
    set(_kstar_check_deps "")
    foreach(t
        kstar_parallel_gtest
        kstar_workflow_synthesized_gtest
        energy_matrix_gtest
        energy_matrix_osprey_pattern_gtest
        energy_matrix_java_comparison_gtest
        astar_gtest
        astar_node_osprey_gtest
        kstar_score_gtest
        partition_function_gtest
        partition_function_synthesized_gtest
        partition_function_precision_tier0_gtest
        partition_function_precision_tier1_gtest
        log_space_gtest
        log_space_precision_tier1_gtest
        conf_index_gtest
        conf_ranker_gtest
        conf_search_astar_synthesized_gtest
        conf_search_cache_gtest
        confspace_test
        energy_matrix_loader_corpus_runner
        conf_search_astar_corpus_runner
        energy_matrix_loader_regression_gtest
        partition_function_regression_gtest
        conf_search_astar_regression_gtest
    )
        if (TARGET ${t})
            list(APPEND _kstar_check_deps ${t})
        endif()
    endforeach()

    add_custom_target(kstar_check
        COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure
        DEPENDS ${_kstar_check_deps}
        WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
        COMMENT "Build all kstar tests, then run: ctest --output-on-failure"
        VERBATIM
    )
endif()

# -----------------------------
# Coverage target (GCC/Clang + lcov/genhtml)
# -----------------------------
# Defined near the end so it can depend on the full set of test binaries.
if (BUILD_TESTING AND KSTAR_ENABLE_COVERAGE)
    find_program(LCOV_EXEC lcov)
    find_program(GENHTML_EXEC genhtml)
    find_program(FIND_EXEC find)

    if (NOT LCOV_EXEC OR NOT GENHTML_EXEC)
        message(STATUS "Coverage requested (KSTAR_ENABLE_COVERAGE=ON) but lcov/genhtml not found. Install with: sudo apt-get install -y lcov")
        add_custom_target(kstar_coverage
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "ERROR: lcov/genhtml not found."
            COMMAND ${CMAKE_COMMAND} -E echo "Install with: sudo apt-get install -y lcov"
            COMMAND ${CMAKE_COMMAND} -E false
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            VERBATIM
        )
    else()
        # Produce multiple coverage "mixtures" from the same instrumented run.
        # - all: production + tests
        # - prod: production only (filters out src/test/**)
        # - tests: tests only
        set(_kstar_cov_raw        "coverage.raw.info")
        set(_kstar_cov_all        "coverage.all.info")
        set(_kstar_cov_prod       "coverage.prod.info")
        set(_kstar_cov_tests      "coverage.tests.info")

        # Filters:
        # - Exclude system headers, fetched deps, and googletest itself.
        # - Exclude all repo tests for the prod-only report.
        set(_kstar_cov_excludes_common
            "/usr/*"
            "${CMAKE_BINARY_DIR}/_deps/*"
            "*googletest*"
            "*gtest*"
        )
        set(_kstar_cov_excludes_tests
            "*/src/test/*"
        )
        set(_kstar_cov_extract_tests
            "*/src/test/*"
        )

        # Backwards-compatible target name: kstar_coverage == "all" (prod + tests)
        add_custom_target(kstar_coverage_all
            # Avoid gcov "overwriting existing profile data with a different timestamp" by deleting stale .gcda first.
            # (This can happen when running ctest directly between builds or after recompiles.)
            COMMAND ${CMAKE_COMMAND} -E echo "Resetting coverage counters..."
            COMMAND $<$<BOOL:${FIND_EXEC}>:${FIND_EXEC}> . -name "*.gcda" -delete
            COMMAND ${LCOV_EXEC} --directory . --zerocounters
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure

            # Capture, then do filtering into the three output tracefiles.
            COMMAND ${LCOV_EXEC} --capture --directory . --output-file "${_kstar_cov_raw}"
                --base-directory "${REPO_ROOT}"
                --rc lcov_branch_coverage=1

            # all (prod + tests)
            COMMAND ${LCOV_EXEC} --remove "${_kstar_cov_raw}" ${_kstar_cov_excludes_common}
                --output-file "${_kstar_cov_all}"
                --rc lcov_branch_coverage=1
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage/kstar-all"
            COMMAND ${GENHTML_EXEC} -o "${CMAKE_BINARY_DIR}/coverage/kstar-all" "${_kstar_cov_all}"
                --branch-coverage

            # prod-only
            COMMAND ${LCOV_EXEC} --remove "${_kstar_cov_all}" ${_kstar_cov_excludes_tests}
                --output-file "${_kstar_cov_prod}"
                --rc lcov_branch_coverage=1
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage/kstar-prod"
            COMMAND ${GENHTML_EXEC} -o "${CMAKE_BINARY_DIR}/coverage/kstar-prod" "${_kstar_cov_prod}"
                --branch-coverage

            # tests-only
            COMMAND ${LCOV_EXEC} --extract "${_kstar_cov_all}" ${_kstar_cov_extract_tests}
                --output-file "${_kstar_cov_tests}"
                --rc lcov_branch_coverage=1
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage/kstar-tests"
            COMMAND ${GENHTML_EXEC} -o "${CMAKE_BINARY_DIR}/coverage/kstar-tests" "${_kstar_cov_tests}"
                --branch-coverage

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Running tests and generating coverage HTML reports (all/prod/tests) under ${CMAKE_BINARY_DIR}/coverage/"
            VERBATIM
        )
        add_dependencies(kstar_coverage_all ${_kstar_check_deps})

        add_custom_target(kstar_coverage DEPENDS kstar_coverage_all)

        # Convenience single-mixture targets (useful for CI)
        add_custom_target(kstar_coverage_prod
            DEPENDS kstar_coverage_all
            COMMENT "Alias: generate all mixtures, then use the prod-only report at ${CMAKE_BINARY_DIR}/coverage/kstar-prod/index.html"
        )
        add_custom_target(kstar_coverage_tests
            DEPENDS kstar_coverage_all
            COMMENT "Alias: generate all mixtures, then use the tests-only report at ${CMAKE_BINARY_DIR}/coverage/kstar-tests/index.html"
        )

        # A/B compare: prod-only coverage WITHOUT vs WITH fuzz corpus replay.
        # Baseline excludes the `fuzz_corpus` label (i.e., excludes kstar.energy_matrix_loader_corpus_runner).
        add_custom_target(kstar_coverage_prod_compare_fuzz
            # --- baseline (no fuzz_corpus) ---
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "=== kstar-prod baseline (excluding label: fuzz_corpus) ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Resetting coverage counters..."
            COMMAND $<$<BOOL:${FIND_EXEC}>:${FIND_EXEC}> . -name "*.gcda" -delete
            COMMAND ${LCOV_EXEC} --directory . --zerocounters
            COMMAND ${CMAKE_CTEST_COMMAND} --output-on-failure -LE fuzz_corpus
            COMMAND ${LCOV_EXEC} --capture --directory . --output-file coverage.raw.baseline.info
                --base-directory "${REPO_ROOT}"
                --rc lcov_branch_coverage=1
            COMMAND ${LCOV_EXEC} --remove coverage.raw.baseline.info ${_kstar_cov_excludes_common}
                --output-file coverage.all.baseline.info
                --rc lcov_branch_coverage=1
            COMMAND ${LCOV_EXEC} --remove coverage.all.baseline.info ${_kstar_cov_excludes_tests}
                --output-file coverage.prod.baseline.info
                --rc lcov_branch_coverage=1
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage/kstar-prod-baseline"
            COMMAND ${GENHTML_EXEC} -o "${CMAKE_BINARY_DIR}/coverage/kstar-prod-baseline" coverage.prod.baseline.info
                --branch-coverage

            # --- with fuzz corpus replay ---
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "=== kstar-prod with fuzz corpus replay (includes label: fuzz_corpus) ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Resetting coverage counters..."
            COMMAND $<$<BOOL:${FIND_EXEC}>:${FIND_EXEC}> . -name "*.gcda" -delete
            COMMAND ${LCOV_EXEC} --directory . --zerocounters
            COMMAND ${CMAKE_COMMAND} -E env KSTAR_CORPUS_RUNNER_STRICT=1 ${CMAKE_CTEST_COMMAND} --output-on-failure
            COMMAND ${LCOV_EXEC} --capture --directory . --output-file coverage.raw.fuzz.info
                --base-directory "${REPO_ROOT}"
                --rc lcov_branch_coverage=1
            COMMAND ${LCOV_EXEC} --remove coverage.raw.fuzz.info ${_kstar_cov_excludes_common}
                --output-file coverage.all.fuzz.info
                --rc lcov_branch_coverage=1
            COMMAND ${LCOV_EXEC} --remove coverage.all.fuzz.info ${_kstar_cov_excludes_tests}
                --output-file coverage.prod.fuzz.info
                --rc lcov_branch_coverage=1
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage/kstar-prod-fuzz"
            COMMAND ${GENHTML_EXEC} -o "${CMAKE_BINARY_DIR}/coverage/kstar-prod-fuzz" coverage.prod.fuzz.info
                --branch-coverage

            # --- delta report (stable, machine-readable) ---
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "=== prod-only coverage delta (baseline vs fuzz) ==="
            COMMAND /bin/bash -lc
                "${REPO_ROOT}/scripts/kstar_coverage_prod_delta_report.sh ${CMAKE_BINARY_DIR}/coverage.prod.baseline.info ${CMAKE_BINARY_DIR}/coverage.prod.fuzz.info ${CMAKE_BINARY_DIR}/coverage/prod_coverage_delta.tsv"
            COMMAND ${CMAKE_COMMAND} -E echo "Top per-file deltas (first 15 lines):"
            COMMAND /bin/bash -lc
                "head -n 16 ${CMAKE_BINARY_DIR}/coverage/prod_coverage_delta.tsv || true"
            COMMAND ${CMAKE_COMMAND} -E echo "Summary: ${CMAKE_BINARY_DIR}/coverage/prod_coverage_delta.summary.md"
            COMMAND ${CMAKE_COMMAND} -E echo "Top newly covered branches/lines/functions are also written under ${CMAKE_BINARY_DIR}/coverage/:"
            COMMAND ${CMAKE_COMMAND} -E echo "  - prod_coverage_delta.new_branches.tsv"
            COMMAND ${CMAKE_COMMAND} -E echo "  - prod_coverage_delta.new_lines.tsv"
            COMMAND ${CMAKE_COMMAND} -E echo "  - prod_coverage_delta.new_functions.tsv"

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Generate A/B prod-only coverage reports: baseline vs with fuzz corpus replay"
            VERBATIM
        )
        add_dependencies(kstar_coverage_prod_compare_fuzz ${_kstar_check_deps})

        # Optional: gcovr reports (Cobertura XML + HTML). Useful for tooling/CI ingestion.
        # NOTE: resolve gcovr at *build/run time* so installing it doesn't require re-running cmake configure.
        # This reuses the same gcov-instrumented build tree and .gcda produced by running tests.
        add_custom_target(kstar_coverage_gcovr
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "=== gcovr reports (Cobertura XML + HTML) ==="
            COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/coverage-gcovr"

            COMMAND /bin/bash -lc
                "command -v gcovr >/dev/null 2>&1 || { echo 'ERROR: gcovr not found.'; echo 'Install with: pip install gcovr   (or: sudo apt-get install -y gcovr)'; exit 2; }"

            # Cobertura XML (prod-only by default: exclude src/test/** and common externals)
            COMMAND /bin/bash -lc
                "gcovr -r \"${REPO_ROOT}\" --object-directory \"${CMAKE_BINARY_DIR}\" \
                  --exclude \".*?/src/test/.*\" \
                  --exclude \".*/_deps/.*\" \
                  --exclude \".*/googletest/.*\" \
                  --exclude \".*/gtest/.*\" \
                  --exclude \"/usr/.*\" \
                  --branches --xml-pretty \
                  -o \"${CMAKE_BINARY_DIR}/coverage-gcovr/coverage.prod.cobertura.xml\""

            # HTML (same filtering)
            COMMAND /bin/bash -lc
                "gcovr -r \"${REPO_ROOT}\" --object-directory \"${CMAKE_BINARY_DIR}\" \
                  --exclude \".*?/src/test/.*\" \
                  --exclude \".*/_deps/.*\" \
                  --exclude \".*/googletest/.*\" \
                  --exclude \".*/gtest/.*\" \
                  --exclude \"/usr/.*\" \
                  --branches --html-details \
                  -o \"${CMAKE_BINARY_DIR}/coverage-gcovr/index.html\""

            # Bottom-N by branch% (stable summary)
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "Bottom files by branch coverage (prod-only):"
            COMMAND /bin/bash -lc
                "python3 \"${REPO_ROOT}/scripts/kstar_gcovr_summarize_cobertura.py\" \
                  \"${CMAKE_BINARY_DIR}/coverage-gcovr/coverage.prod.cobertura.xml\" \
                  --repo-root \"${REPO_ROOT}\" \
                  --out-tsv \"${CMAKE_BINARY_DIR}/coverage-gcovr/prod_file_branch_bottom.tsv\" \
                  --bottom 15 | head -n 16 || true"

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            DEPENDS kstar_coverage_all
            COMMENT "Generate gcovr Cobertura XML + HTML under ${CMAKE_BINARY_DIR}/coverage-gcovr/"
            VERBATIM
        )
    endif()
endif()

# -----------------------------
# LLVM source-based coverage target (Clang + llvm-profdata/llvm-cov)
# -----------------------------
if (BUILD_TESTING AND KSTAR_ENABLE_LLVM_COVERAGE)
    find_program(LLVM_PROFDATA_EXEC llvm-profdata)
    find_program(LLVM_COV_EXEC llvm-cov)

    if (NOT LLVM_PROFDATA_EXEC OR NOT LLVM_COV_EXEC)
        message(STATUS "LLVM coverage requested (KSTAR_ENABLE_LLVM_COVERAGE=ON) but llvm-profdata/llvm-cov not found. Install with: sudo apt-get install -y llvm")
        add_custom_target(kstar_llvm_coverage
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "ERROR: llvm-profdata/llvm-cov not found."
            COMMAND ${CMAKE_COMMAND} -E echo "Install with: sudo apt-get install -y llvm"
            COMMAND ${CMAKE_COMMAND} -E false
            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            VERBATIM
        )
    else()
        set(_kstar_llvm_profraw_dir "${CMAKE_BINARY_DIR}/llvm-profraw")
        set(_kstar_llvm_profdata "${CMAKE_BINARY_DIR}/coverage.profdata")
        set(_kstar_llvm_html_root "${CMAKE_BINARY_DIR}/coverage-llvm")

        # Common ignore filters: system headers + fetched deps.
        set(_kstar_llvm_ignore_common "(/usr/|.*/_deps/|.*/googletest/|.*/gtest/).*")
        set(_kstar_llvm_ignore_prod "(.*/src/test/.*)|${_kstar_llvm_ignore_common}")

        add_custom_target(kstar_llvm_coverage_all
            COMMAND ${CMAKE_COMMAND} -E echo ""
            COMMAND ${CMAKE_COMMAND} -E echo "=== llvm-cov (Clang source-based coverage): run tests, then render HTML ==="
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${_kstar_llvm_profraw_dir}"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${_kstar_llvm_profdata}"
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_kstar_llvm_profraw_dir}"

            # Run tests with LLVM_PROFILE_FILE so each process writes a unique .profraw.
            COMMAND ${CMAKE_COMMAND} -E env
                # IMPORTANT: do not wrap in quotes; CMake does not invoke a shell here.
                # Quotes would become part of the value and prevent profile files from being created.
                LLVM_PROFILE_FILE=${_kstar_llvm_profraw_dir}/%p-%m.profraw
                ${CMAKE_CTEST_COMMAND} --output-on-failure

            # Merge all profraw into one profdata
            COMMAND /bin/bash -lc
                "set -euo pipefail; \
                 if ! compgen -G \"${_kstar_llvm_profraw_dir}/*.profraw\" >/dev/null; then \
                   echo 'ERROR: no llvm profraw files were written.'; \
                   echo 'Expected files under: ${_kstar_llvm_profraw_dir}/'; \
                   echo 'Most common causes:'; \
                   echo '  - Build was not compiled with clang source-based coverage flags (-fprofile-instr-generate -fcoverage-mapping).'; \
                   echo '  - Tests did not actually execute (all skipped/failed before running instrumented code).'; \
                   echo '  - LLVM_PROFILE_FILE was not applied to the test processes.'; \
                   echo 'Check:'; \
                   echo '  - You configured with: -DCMAKE_CXX_COMPILER=clang++ -DKSTAR_ENABLE_LLVM_COVERAGE=ON'; \
                   echo '  - ctest output shows tests running (not all skipped).'; \
                   exit 2; \
                 fi; \
                 \"${LLVM_PROFDATA_EXEC}\" merge -sparse \"${_kstar_llvm_profraw_dir}\"/*.profraw -o \"${_kstar_llvm_profdata}\""

            # Render ALL (prod + tests)
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_kstar_llvm_html_root}/kstar-all"
            COMMAND ${LLVM_COV_EXEC} show
                -format=html
                -output-dir "${_kstar_llvm_html_root}/kstar-all"
                -instr-profile "${_kstar_llvm_profdata}"
                -ignore-filename-regex "${_kstar_llvm_ignore_common}"
                $<$<TARGET_EXISTS:partition_function_gtest>:-object> $<$<TARGET_EXISTS:partition_function_gtest>:$<TARGET_FILE:partition_function_gtest>>
                $<$<TARGET_EXISTS:kstar_parallel_gtest>:-object> $<$<TARGET_EXISTS:kstar_parallel_gtest>:$<TARGET_FILE:kstar_parallel_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_gtest>:$<TARGET_FILE:energy_matrix_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_osprey_pattern_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_osprey_pattern_gtest>:$<TARGET_FILE:energy_matrix_osprey_pattern_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_java_comparison_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_java_comparison_gtest>:$<TARGET_FILE:energy_matrix_java_comparison_gtest>>
                $<$<TARGET_EXISTS:astar_gtest>:-object> $<$<TARGET_EXISTS:astar_gtest>:$<TARGET_FILE:astar_gtest>>
                $<$<TARGET_EXISTS:astar_node_osprey_gtest>:-object> $<$<TARGET_EXISTS:astar_node_osprey_gtest>:$<TARGET_FILE:astar_node_osprey_gtest>>
                $<$<TARGET_EXISTS:conf_index_gtest>:-object> $<$<TARGET_EXISTS:conf_index_gtest>:$<TARGET_FILE:conf_index_gtest>>
                $<$<TARGET_EXISTS:conf_ranker_gtest>:-object> $<$<TARGET_EXISTS:conf_ranker_gtest>:$<TARGET_FILE:conf_ranker_gtest>>
                $<$<TARGET_EXISTS:conf_search_cache_gtest>:-object> $<$<TARGET_EXISTS:conf_search_cache_gtest>:$<TARGET_FILE:conf_search_cache_gtest>>
                $<$<TARGET_EXISTS:kstar_score_gtest>:-object> $<$<TARGET_EXISTS:kstar_score_gtest>:$<TARGET_FILE:kstar_score_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_loader_corpus_runner>:-object> $<$<TARGET_EXISTS:energy_matrix_loader_corpus_runner>:$<TARGET_FILE:energy_matrix_loader_corpus_runner>>
                $<$<TARGET_EXISTS:energy_matrix_loader_regression_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_loader_regression_gtest>:$<TARGET_FILE:energy_matrix_loader_regression_gtest>>

            # Render PROD-ONLY (filter out src/test/**)
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_kstar_llvm_html_root}/kstar-prod"
            COMMAND ${LLVM_COV_EXEC} show
                -format=html
                -output-dir "${_kstar_llvm_html_root}/kstar-prod"
                -instr-profile "${_kstar_llvm_profdata}"
                -ignore-filename-regex "${_kstar_llvm_ignore_prod}"
                $<$<TARGET_EXISTS:partition_function_gtest>:-object> $<$<TARGET_EXISTS:partition_function_gtest>:$<TARGET_FILE:partition_function_gtest>>
                $<$<TARGET_EXISTS:kstar_parallel_gtest>:-object> $<$<TARGET_EXISTS:kstar_parallel_gtest>:$<TARGET_FILE:kstar_parallel_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_gtest>:$<TARGET_FILE:energy_matrix_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_osprey_pattern_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_osprey_pattern_gtest>:$<TARGET_FILE:energy_matrix_osprey_pattern_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_java_comparison_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_java_comparison_gtest>:$<TARGET_FILE:energy_matrix_java_comparison_gtest>>
                $<$<TARGET_EXISTS:astar_gtest>:-object> $<$<TARGET_EXISTS:astar_gtest>:$<TARGET_FILE:astar_gtest>>
                $<$<TARGET_EXISTS:astar_node_osprey_gtest>:-object> $<$<TARGET_EXISTS:astar_node_osprey_gtest>:$<TARGET_FILE:astar_node_osprey_gtest>>
                $<$<TARGET_EXISTS:conf_index_gtest>:-object> $<$<TARGET_EXISTS:conf_index_gtest>:$<TARGET_FILE:conf_index_gtest>>
                $<$<TARGET_EXISTS:conf_ranker_gtest>:-object> $<$<TARGET_EXISTS:conf_ranker_gtest>:$<TARGET_FILE:conf_ranker_gtest>>
                $<$<TARGET_EXISTS:conf_search_cache_gtest>:-object> $<$<TARGET_EXISTS:conf_search_cache_gtest>:$<TARGET_FILE:conf_search_cache_gtest>>
                $<$<TARGET_EXISTS:kstar_score_gtest>:-object> $<$<TARGET_EXISTS:kstar_score_gtest>:$<TARGET_FILE:kstar_score_gtest>>
                $<$<TARGET_EXISTS:energy_matrix_loader_corpus_runner>:-object> $<$<TARGET_EXISTS:energy_matrix_loader_corpus_runner>:$<TARGET_FILE:energy_matrix_loader_corpus_runner>>
                $<$<TARGET_EXISTS:energy_matrix_loader_regression_gtest>:-object> $<$<TARGET_EXISTS:energy_matrix_loader_regression_gtest>:$<TARGET_FILE:energy_matrix_loader_regression_gtest>>

            WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
            COMMENT "Run tests with LLVM profile instrumentation and generate HTML reports under ${_kstar_llvm_html_root}/"
            VERBATIM
        )
        add_dependencies(kstar_llvm_coverage_all ${_kstar_check_deps})

        add_custom_target(kstar_llvm_coverage DEPENDS kstar_llvm_coverage_all)
    endif()
endif()

