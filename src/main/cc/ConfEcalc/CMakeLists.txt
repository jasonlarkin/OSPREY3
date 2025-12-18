cmake_minimum_required(VERSION 3.10)

project(ConfEcalc VERSION 0.1 DESCRIPTION "native conformation energy calculator for OSPREY")

set(CMAKE_CXX_STANDARD 20)

# let CMake create defines that can use in the code
configure_file(config.h.in config.h)

# define the source files to compile
set(CXXFiles
        confecalc.cc
        )

SET(CMAKE_CXX_FLAGS -pedantic-errors)

# OpenMP threading option
option(ENABLE_OPENMP "Enable OpenMP threading for parallel execution" ON)
if(ENABLE_OPENMP)
    find_package(OpenMP)
    if(OpenMP_CXX_FOUND)
        message(STATUS "OpenMP found: ${OpenMP_CXX_VERSION}")
        add_definitions(-DUSE_OPENMP)
    else()
        message(WARNING "OpenMP not found - threading disabled")
        set(ENABLE_OPENMP OFF)
    endif()
endif()

# SIMD optimization option
#
# Important: do NOT compile the entire library with ISA-specific flags like -mavx2/-mfma/-mavx512*.
# Those flags can change scalar numerical behavior (eg via FMA contraction or different libm/codegen),
# which breaks tight-tolerance regression tests. SIMD code is implemented via intrinsics in headers
# and uses per-function target attributes; keep the baseline compilation generic.
option(ENABLE_SIMD "Enable SIMD optimizations with runtime CPU detection" ON)

if(ENABLE_SIMD)
    include(CheckCXXCompilerFlag)
    
    # Build with all SIMD versions simultaneously
    # Runtime CPU detection will select the best available version
    add_definitions(-DUSE_SIMD)
    # Important for reproducibility:
    # enabling -mfma can change scalar rounding via FMA contraction. Disable contraction so
    # scalar paths remain stable while SIMD code uses explicit intrinsics where desired.
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ffp-contract=off")
    
    # Check and enable AVX-512 if compiler supports it
    check_cxx_compiler_flag("-mavx512f" COMPILER_SUPPORTS_AVX512F)
    check_cxx_compiler_flag("-mavx512dq" COMPILER_SUPPORTS_AVX512DQ)
    if(COMPILER_SUPPORTS_AVX512F AND COMPILER_SUPPORTS_AVX512DQ)
        add_definitions(-DUSE_AVX512)
        message(STATUS "AVX-512 support enabled (will be used if CPU supports it)")
    else()
        message(STATUS "AVX-512 not supported by compiler (will skip AVX-512 version)")
    endif()
    
    # Check and enable AVX2 if compiler supports it
    check_cxx_compiler_flag("-mavx2" COMPILER_SUPPORTS_AVX2)
    if(COMPILER_SUPPORTS_AVX2)
        message(STATUS "AVX2 support enabled (will be used if CPU supports it)")
    else()
        message(WARNING "Compiler does not support AVX2, only scalar version will be available")
    endif()
    
    message(STATUS "SIMD optimizations enabled with runtime CPU detection")
    message(STATUS "  - Runtime will automatically select: AVX-512 → AVX2 → Scalar")
else()
    message(STATUS "SIMD optimizations disabled")
endif()

# get paths into the parent Osprey project
set(OspreyPath ${CMAKE_SOURCE_DIR}/../../../..)
set(JnaPlatform linux-x86-64) # TODO: get the JNA names for each platform
#set(JnaPlatform darwin-aarch64) #for apple silicon macs
set(OspreyBuildPath ${OspreyPath}/build/resources/main/${JnaPlatform})

# IMPORTANT:
# Do NOT write build outputs into the source tree (eg src/main/resources/**), otherwise every
# local native rebuild dirties git (and can accidentally commit a machine-specific binary).
# For local development/tests, write directly into Gradle's build resources dir.
file(MAKE_DIRECTORY ${OspreyBuildPath})

# make a library target
add_library(ConfEcalc SHARED ${CXXFiles})
target_include_directories(ConfEcalc PUBLIC "${PROJECT_BINARY_DIR}")
set_target_properties(ConfEcalc PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${OspreyBuildPath}"
)
if(ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(ConfEcalc PUBLIC OpenMP::OpenMP_CXX)
endif()

# add another target to copy the libraries from the build path into the source-tree resources dir
# (useful only when you intentionally want to update the committed/prebuilt JNA library)
set(OspreyResourcesPath ${OspreyPath}/src/main/resources/${JnaPlatform})
file(MAKE_DIRECTORY ${OspreyResourcesPath})
add_custom_target(ConfEcalc_CopyLibs)
add_dependencies(ConfEcalc_CopyLibs ConfEcalc)
add_custom_command(
        TARGET ConfEcalc_CopyLibs POST_BUILD
        COMMAND cp ${OspreyBuildPath}/$<TARGET_FILE_NAME:ConfEcalc> ${OspreyResourcesPath}
)

# Direct SIMD benchmark (always available)
add_executable(benchmark_simd_direct
    tests/benchmark_simd_direct.cpp
)
target_include_directories(benchmark_simd_direct PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
# Link against the same flags as the library
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_simd_direct PRIVATE USE_SIMD)
endif()

# Parameter sweep benchmark
add_executable(benchmark_simd_parameter_sweep
    tests/benchmark_simd_parameter_sweep.cpp
)
target_include_directories(benchmark_simd_parameter_sweep PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_simd_parameter_sweep PRIVATE USE_SIMD)
endif()

# Separate benchmarks for accurate profiling
add_executable(benchmark_scalar_only
    tests/benchmark_scalar_only.cpp
)
target_include_directories(benchmark_scalar_only PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
target_link_libraries(benchmark_scalar_only ConfEcalc)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_scalar_only PRIVATE USE_SIMD)
endif()

add_executable(benchmark_avx2_only
    tests/benchmark_avx2_only.cpp
)
target_include_directories(benchmark_avx2_only PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
target_link_libraries(benchmark_avx2_only ConfEcalc)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_avx2_only PRIVATE USE_SIMD)
endif()

add_executable(benchmark_avx512_only
    tests/benchmark_avx512_only.cpp
)
target_include_directories(benchmark_avx512_only PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
target_link_libraries(benchmark_avx512_only ConfEcalc)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_avx512_only PRIVATE USE_SIMD)
endif()

# Compute-bound benchmark (matrix multiplication - high arithmetic intensity)
add_executable(benchmark_compute_bound tests/benchmark_compute_bound.cpp)
target_include_directories(benchmark_compute_bound PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_compute_bound PRIVATE USE_SIMD)
    # This benchmark uses AVX2/FMA intrinsics directly, so it must be compiled with ISA flags.
    if(COMPILER_SUPPORTS_AVX2)
        target_compile_options(benchmark_compute_bound PRIVATE -mavx2 -mfma)
    endif()
    if(COMPILER_SUPPORTS_AVX512F AND COMPILER_SUPPORTS_AVX512DQ)
        target_compile_definitions(benchmark_compute_bound PRIVATE USE_AVX512)
        target_compile_options(benchmark_compute_bound PRIVATE -mavx512f -mavx512dq -mfma)
    endif()
endif()

# Rotation operations benchmark
add_executable(benchmark_rotation_operations tests/benchmark_rotation_operations.cpp)
target_include_directories(benchmark_rotation_operations PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
target_link_libraries(benchmark_rotation_operations ConfEcalc)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_rotation_operations PRIVATE USE_SIMD)
    if(COMPILER_SUPPORTS_AVX512F AND COMPILER_SUPPORTS_AVX512DQ)
        target_compile_definitions(benchmark_rotation_operations PRIVATE USE_AVX512)
    endif()
endif()

# TranslationRotation transform benchmark
add_executable(benchmark_transrot tests/benchmark_transrot.cpp)
target_include_directories(benchmark_transrot PRIVATE 
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
target_link_libraries(benchmark_transrot ConfEcalc)
if(ENABLE_SIMD)
    target_compile_definitions(benchmark_transrot PRIVATE USE_SIMD)
    if(COMPILER_SUPPORTS_AVX512F AND COMPILER_SUPPORTS_AVX512DQ)
        target_compile_definitions(benchmark_transrot PRIVATE USE_AVX512)
    endif()
endif()

# CCD Minimization threading benchmark
add_executable(benchmark_minimization_ccd tests/benchmark_minimization_ccd.cpp)
target_include_directories(benchmark_minimization_ccd PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${PROJECT_BINARY_DIR}"
)
target_link_libraries(benchmark_minimization_ccd ConfEcalc)
if(ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(benchmark_minimization_ccd OpenMP::OpenMP_CXX)
    target_compile_definitions(benchmark_minimization_ccd PRIVATE USE_OPENMP)
endif()

# Add tests subdirectory (optional, only if building tests)
option(BUILD_TESTS "Build C++ tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
