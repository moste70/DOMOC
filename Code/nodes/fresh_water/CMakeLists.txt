cmake_minimum_required(VERSION 3.16)

# Resolve path to Base component - use file(REAL_PATH) for explicit resolution
file(REAL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../../Base" BASE_COMPONENT_PATH_ABS)
set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_ABS}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(fresh_water_node)
