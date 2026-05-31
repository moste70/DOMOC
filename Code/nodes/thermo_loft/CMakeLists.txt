cmake_minimum_required(VERSION 3.16)

# Resolve path to Base component using absolute path
get_filename_component(BASE_COMPONENT_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/../../Base"
    ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH}")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermo_loft_node)
