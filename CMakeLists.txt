# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

set(PROJECT_VER 1.2.0.0)

# This example uses an extra component.
set(EXTRA_COMPONENT_DIRS C:/Users/AppDev/esp/esp-idf-lib/components)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)


project(HydroESPro_v1)

idf_build_set_property(COMPILE_OPTIONS "-I../main" APPEND)

idf_build_set_property(COMPILE_OPTIONS "-D_PROJECT_NAME_=\"${CMAKE_PROJECT_NAME}\"" APPEND)
idf_build_set_property(COMPILE_OPTIONS "-D_PROJECT_VER_=\"${PROJECT_VER}\"" APPEND)
