cmake_minimum_required(VERSION 3.16)

# Build path to Base component
set(BASE_COMPONENT_PATH_1 "${CMAKE_CURRENT_SOURCE_DIR}/../../Base")
set(BASE_COMPONENT_PATH_2 "${CMAKE_CURRENT_SOURCE_DIR}/../../../Code/Base")

# Use approach 1, but verify it exists
if(EXISTS "${BASE_COMPONENT_PATH_1}/CMakeLists.txt")
    set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_1}")
elseif(EXISTS "${BASE_COMPONENT_PATH_2}/CMakeLists.txt")
    set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_2}")
else()
    message(WARNING "Base component not found!")
    set(EXTRA_COMPONENT_DIRS "${BASE_COMPONENT_PATH_1}")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(thermo_bunk_node)
