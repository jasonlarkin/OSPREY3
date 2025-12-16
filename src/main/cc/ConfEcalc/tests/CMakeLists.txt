# Google Test setup
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Testing enabled in parent CMakeLists.txt

# Test executables
add_executable(confecalc_concepts_tests
    test_concepts_gtest.cpp
)

add_executable(confecalc_all_features_tests
    test_all_features_gtest.cpp
)

add_executable(confecalc_nodiscard_tests
    test_nodiscard_gtest.cpp
)

add_executable(confecalc_compile_failures_tests
    test_concepts_compile_failures_gtest.cpp
)

add_executable(confecalc_array_tests
    test_array_gtest.cpp
)

add_executable(confecalc_concepts_advanced_tests
    test_concepts_advanced_gtest.cpp
)

# Common link libraries and include directories
set(COMMON_LIBS
    PRIVATE
    ConfEcalc
    GTest::gtest
    GTest::gtest_main
)

set(COMMON_INCLUDES
    PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_BINARY_DIR}
)

# Link and configure each test executable
foreach(TARGET confecalc_concepts_tests confecalc_all_features_tests confecalc_nodiscard_tests confecalc_compile_failures_tests confecalc_array_tests confecalc_concepts_advanced_tests)
    target_link_libraries(${TARGET} ${COMMON_LIBS})
    target_include_directories(${TARGET} ${COMMON_INCLUDES})
endforeach()

# Discover tests for CTest
include(GoogleTest)
gtest_discover_tests(confecalc_concepts_tests
    DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
)
gtest_discover_tests(confecalc_all_features_tests
    DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
)
gtest_discover_tests(confecalc_nodiscard_tests
    DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
)
gtest_discover_tests(confecalc_compile_failures_tests
    DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
)
gtest_discover_tests(confecalc_array_tests
    DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
)
gtest_discover_tests(confecalc_concepts_advanced_tests
    DISCOVER_TESTS_DISCOVERED_BY_CMAKE TRUE
)

