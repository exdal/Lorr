cmake_minimum_required(VERSION 3.20)

include(CMake/LorrComponent.cmake)

project(Lorr VERSION 0.1.0 LANGUAGES CXX C)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# add_definitions(-DTRACY_ENABLE)

add_definitions(-DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

# ASAN SETTINGS
# add_definitions(-D_DISABLE_STRING_ANNOTATION)
# add_definitions(-D_DISABLE_VECTOR_ANNOTATION)
# add_compile_options(-fsanitize=address)
# add_link_options(-fsanitize=address)

add_subdirectory(Vendor)
add_subdirectory(Lorr)

add_subdirectory(Examples)
