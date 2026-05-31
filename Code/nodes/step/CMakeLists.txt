cmake_minimum_required(VERSION 3.16)

# Use local copy of Base component in components_local/
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_SOURCE_DIR}/components_local")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(step_node)
