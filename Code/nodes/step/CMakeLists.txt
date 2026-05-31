cmake_minimum_required(VERSION 3.16)

# Build path to Base component - try multiple approaches
# Approach 1: Relative to current source dir
set(BASE_COMPONENT_PATH_1 "${CMAKE_CURRENT_SOURCE_DIR}/../../Base")

# Approach 2: Relative to top-level project dir
set(BASE_COMPONENT_PATH_2 "${CMAKE_CURRENT_SOURCE_DIR}/../../../Code/Base")

# Use approach 1, but verify it exists before include()
if(EXISTS "${BASE_COMPONENT_PATH_1}/CMakeLists.txt")
    set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_1}")
elseif(EXISTS "${BASE_COMPONENT_PATH_2}/CMakeLists.txt")
    set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_2}")
else()
    message(WARNING "Base component not found!")
    # Still set it so we get a proper error message
    set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_1}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(step_node)
