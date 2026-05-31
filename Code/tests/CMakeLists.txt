cmake_minimum_required(VERSION 3.16)
project(domoc_tests)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Download and include Google Test
include(FetchContent)
FetchContent_Declare(googletest
  URL https://github.com/google/googletest/archive/v1.14.0.zip
  DOWNLOAD_EXTRACT_TIMESTAMP ON
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)

# Quietly make available
FetchContent_MakeAvailable(googletest)

# Test executable with absolute include paths
add_executable(domoc_tests
    test_protocol.cpp
)

# Use target_include_directories (more modern approach)
target_include_directories(domoc_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../Base/include
)

target_link_libraries(domoc_tests
    gtest_main
)

enable_testing()
add_test(NAME DomoC_Protocol COMMAND domoc_tests)
