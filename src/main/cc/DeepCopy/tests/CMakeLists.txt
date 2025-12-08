# Test executable for DeepCopy
add_executable(deepcopy_tests
    unit/test_basic.cpp
    unit/test_stream_parser.cpp
    unit/test_class_descriptor.cpp
    unit/test_field_parsing.cpp
    unit/test_algorithm.cpp
    unit/test_reference_resolution.cpp
)

target_link_libraries(deepcopy_tests
    PRIVATE
        DeepCopy
        GTest::gtest
        GTest::gtest_main
)

# Ensure DeepCopy library is built before tests
add_dependencies(deepcopy_tests DeepCopy)

target_include_directories(deepcopy_tests
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_BINARY_DIR}
)

# Discover tests for CTest
include(GoogleTest)
gtest_discover_tests(deepcopy_tests)

