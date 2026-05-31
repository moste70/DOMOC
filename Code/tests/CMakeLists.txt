cmake_minimum_required(VERSION 3.16)
project(domoc_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download and include Google Test
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Include the DomoC headers
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/..)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../Base/include)

# Test executable
add_executable(domoc_tests
    test_protocol.cpp
)

target_link_libraries(domoc_tests
    gtest_main
)

enable_testing()
add_test(NAME DomoC_Protocol COMMAND domoc_tests)
