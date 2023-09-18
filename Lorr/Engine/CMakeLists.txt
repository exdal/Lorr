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

# Core
LorrComponent(
    NAME
        Lorr
    SOURCE
        ./*.cc
    VENDOR
        ${LORR_VENDOR}
    PCH
        pch.hh
    INCLUDE_PUB
        ./
        ../../
    LIBTYPE
        STATIC
)