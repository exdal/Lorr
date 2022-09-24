cmake_minimum_required (VERSION 3.5)

project (LuaBridge)

include (CMakeDependentOption)

option (LUABRIDGE_CXX17 "Use C++17 standard if supported by compiler" OFF)

if (LUABRIDGE_CXX17)
  set (CMAKE_CXX_STANDARD 17)
else ()
  set (CMAKE_CXX_STANDARD 11)
endif ()

set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory (Source)