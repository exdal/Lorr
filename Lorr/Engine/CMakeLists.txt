set(LORR_VENDOR
    EASTL
    EAThread
    DirectXMath
    lw
    TracyClient

    fmt
    spdlog

    glslang 
    OGLCompiler 
    OSDependent 
    SPIRV 
    Vulkan-Headers
    imgui
    simdjson::simdjson
    fastgltf

    xxhash
)

file(GLOB_RECURSE LORR_SOURCE
    *.cc
)

add_library(Lorr STATIC ${LORR_SOURCE})
target_include_directories(Lorr PUBLIC ./ ../../)
target_link_libraries(Lorr PUBLIC ${LORR_VENDOR})
target_precompile_headers(Lorr PUBLIC pch.hh)
target_compile_options(Lorr PRIVATE -Wno-c++20-extensions -Wno-c99-designator)