set(LORR_VENDOR
    EASTL
    EAThread
    DirectXMath
    lw
    fmt
    spdlog
    tracy
    
    Vulkan-Headers
    imgui
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
    INCLUDE
        ./
    LIBTYPE
        STATIC
)