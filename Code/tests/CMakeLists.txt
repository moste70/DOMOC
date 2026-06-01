cmake_minimum_required(VERSION 3.16)
project(domoc_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Google Test from system packages
find_package(GTest REQUIRED)

# Test executable
add_executable(domoc_tests
    test_protocol.cpp
)

# Include directories for DomoC headers
target_include_directories(domoc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/include
)

# Link against GTest
target_link_libraries(domoc_tests
    GTest::gtest_main
    GTest::gtest
)

enable_testing()
add_test(NAME DomoC_Protocol COMMAND domoc_tests)
