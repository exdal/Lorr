set (CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH};${CMAKE_CURRENT_SOURCE_DIR}/EA/EASTL/scripts/CMake")
add_subdirectory(EA/EABase)
add_subdirectory(EA/EAAssert)
add_subdirectory(EA/EAStdC)
add_subdirectory(EA/EAThread)
add_subdirectory(EA/EASTL)

add_subdirectory(fmt)

set(TRACY_ENABLE OFF CACHE BOOL "" FORCE)
set(FASTGLTF_DOWNLOAD_SIMDJSON OFF CACHE BOOL "" FORCE)

add_subdirectory(tracy)
add_subdirectory(simdjson)
add_subdirectory(fastgltf)
add_subdirectory(imgui)
add_subdirectory(Vulkan-Headers)
add_subdirectory(xxh)

add_definitions(-DGLM_FORCE_CXX2A)
add_subdirectory(glm)

set(SPIRV_CROSS_ENABLE_GLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_MSL OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
add_subdirectory(SPIRV-Cross)

add_subdirectory(vk-bootstrap)
