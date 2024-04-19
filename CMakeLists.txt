cmake_minimum_required(VERSION 3.27)

project(Lorr VERSION 1.0.0 LANGUAGES CXX C)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

if (LR_ENABLE_ASAN)
    # To run asan, clang-cl needs to be selected
    # Don't forget to set build type to RelWithDebInfo
    add_compile_options(-Wno-unsafe-buffer-usage)
    add_compile_options(-fsanitize=address)
    add_compile_options(/MT)
    link_libraries(clang_rt.asan-x86_64)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_definitions(-DNOMINMAX)
add_definitions(-D_CRT_SECURE_NO_WARNINGS)

add_subdirectory(Vendor)
add_subdirectory(Lorr)

add_subdirectory(Examples)
