# CMake build for generalized molecular energy and search interfaces
cmake_minimum_required(VERSION 3.16)
project(next-interfaces VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find packages (if any dependencies)
# find_package(...)

# Interface library (header-only)
add_library(next-interfaces INTERFACE)
target_include_directories(next-interfaces INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# OSPREY adapter library
add_library(next-osprey-adapters
    adapters/osprey/EnergyMatrixTerm.cpp
    adapters/osprey/RotamerConformation.cpp
    adapters/osprey/RotamerMove.cpp
    adapters/osprey/RotamerNeighborhood.cpp
)

target_link_libraries(next-osprey-adapters PUBLIC next-interfaces)
target_include_directories(next-osprey-adapters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Rosetta adapter library
add_library(next-rosetta-adapters
    adapters/rosetta/ScoreFunctionEvaluator.cpp
    adapters/rosetta/PoseState.cpp
    adapters/rosetta/MoverNeighborhood.cpp
)

target_link_libraries(next-rosetta-adapters PUBLIC next-interfaces)
target_include_directories(next-rosetta-adapters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# OpenMM adapter library
add_library(next-openmm-adapters
    adapters/openmm/ForceTerm.cpp
    adapters/openmm/SystemWrapper.cpp
    adapters/openmm/ContextState.cpp
)

target_link_libraries(next-openmm-adapters PUBLIC next-interfaces)
target_include_directories(next-openmm-adapters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Open Babel adapter library
add_library(next-openbabel-adapters
    adapters/openbabel/OBForceFieldTerm.cpp
    adapters/openbabel/OBConformerNeighborhood.cpp
)

target_link_libraries(next-openbabel-adapters PUBLIC next-interfaces)
target_include_directories(next-openbabel-adapters PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

# Enable warnings
if(MSVC)
    target_compile_options(next-osprey-adapters PRIVATE /W4)
    target_compile_options(next-rosetta-adapters PRIVATE /W4)
    target_compile_options(next-openmm-adapters PRIVATE /W4)
    target_compile_options(next-openbabel-adapters PRIVATE /W4)
else()
    target_compile_options(next-osprey-adapters PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(next-rosetta-adapters PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(next-openmm-adapters PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(next-openbabel-adapters PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Example/demo executables (optional)
add_executable(interface-demo demo/main.cpp)
target_link_libraries(interface-demo PRIVATE next-osprey-adapters)

add_executable(interface-demo-all demo/main_all_adapters.cpp)
target_link_libraries(interface-demo-all PRIVATE 
    next-osprey-adapters 
    next-rosetta-adapters 
    next-openmm-adapters 
    next-openbabel-adapters
)

# Installation
install(TARGETS 
    next-interfaces 
    next-osprey-adapters 
    next-rosetta-adapters 
    next-openmm-adapters 
    next-openbabel-adapters
    EXPORT next-interfaces-targets
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    INCLUDES DESTINATION include
)

install(DIRECTORY include/ DESTINATION include)
install(EXPORT next-interfaces-targets
    FILE next-interfaces-config.cmake
    NAMESPACE next::
    DESTINATION lib/cmake/next-interfaces
)