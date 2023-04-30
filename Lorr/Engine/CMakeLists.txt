set(LORR_VENDOR
    EASTL
    EAThread
    DirectXMath
    lw
    tracy

    fmt

    glslang 
    OGLCompiler 
    OSDependent 
    SPIRV 
    Vulkan-Headers
    imgui
    tinygltf
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