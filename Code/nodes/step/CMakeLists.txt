cmake_minimum_required(VERSION 3.16)
# CMAKE_CURRENT_LIST_DIR must be used to resolve path correctly when cmake runs from build/
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../Base")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(step_node)
