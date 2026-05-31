cmake_minimum_required(VERSION 3.16)

# Debug: print current directory info
message(STATUS "CMAKE_CURRENT_SOURCE_DIR: ${CMAKE_CURRENT_SOURCE_DIR}")

# Resolve path to Base component
set(BASE_COMPONENT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../Base")
message(STATUS "BASE_COMPONENT_PATH (before ABSOLUTE): ${BASE_COMPONENT_PATH}")

get_filename_component(BASE_COMPONENT_PATH_ABS "${BASE_COMPONENT_PATH}" ABSOLUTE)
message(STATUS "BASE_COMPONENT_PATH (after ABSOLUTE): ${BASE_COMPONENT_PATH_ABS}")

# Check if Base component exists
if(EXISTS "${BASE_COMPONENT_PATH_ABS}")
    message(STATUS "Base component found at: ${BASE_COMPONENT_PATH_ABS}")
else()
    message(FATAL_ERROR "Base component NOT found at: ${BASE_COMPONENT_PATH_ABS}")
endif()

set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_ABS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(step_node)
