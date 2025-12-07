cmake_minimum_required(VERSION 3.10)

project(DeepCopy VERSION 0.1 DESCRIPTION "iterative deep copy for OSPREY")

set(CMAKE_CXX_STANDARD 17)

# Define the source files to compile
set(CXXFiles
    deepcopy.cc
    deserializer.cc
    object_graph.cc
)

SET(CMAKE_CXX_FLAGS -pedantic-errors)

# Get paths into the parent Osprey project
set(OspreyPath ${CMAKE_SOURCE_DIR}/../../../..)
set(JnaPlatform linux-x86-64)
set(OspreyBuildPath ${OspreyPath}/build/resources/main/${JnaPlatform})

# Tell CMake to write the libs into Osprey's resources dir
set(LIBRARY_OUTPUT_PATH ${OspreyPath}/src/main/resources/${JnaPlatform})

# Make a library target
add_library(DeepCopy SHARED ${CXXFiles})
target_include_directories(DeepCopy PUBLIC "${PROJECT_BINARY_DIR}")

# Add another target to copy the libraries from the resources path to the build path
add_custom_target(DeepCopy_CopyLibs)
add_dependencies(DeepCopy_CopyLibs DeepCopy)
add_custom_command(
    TARGET DeepCopy_CopyLibs POST_BUILD
    COMMAND cp ${LIBRARY_OUTPUT_PATH}/$<TARGET_FILE_NAME:DeepCopy> ${OspreyBuildPath}
)

# Testing support
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG release-1.12.1
    )
    FetchContent_MakeAvailable(googletest)
    
    enable_testing()
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/tests ${CMAKE_BINARY_DIR}/tests)
endif()

