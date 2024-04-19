find_package(Vulkan REQUIRED)
add_library(dxc UNKNOWN IMPORTED)
set_target_properties(dxc PROPERTIES IMPORTED_LOCATION $ENV{VULKAN_SDK}/Lib/dxcompiler.lib)
set_property(TARGET dxc PROPERTY INTERFACE_INCLUDE_DIRECTORIES $ENV{VULKAN_SDK}/Include)

set(LORR_VENDOR
        TracyClient
        fmt
        Vulkan-Headers
        VulkanMemoryAllocator
        imgui
        simdjson::simdjson
        fastgltf
        xxhash
        slang
        glm::glm
        vk-bootstrap
)

file(GLOB_RECURSE LORR_SOURCE
        *.cc
)

add_library(Lorr STATIC ${LORR_SOURCE})
target_include_directories(Lorr PUBLIC ./ ../../)
target_link_libraries(Lorr PUBLIC ${LORR_VENDOR})
target_precompile_headers(Lorr PUBLIC pch.hh)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(Lorr PUBLIC -march=native)
endif ()
