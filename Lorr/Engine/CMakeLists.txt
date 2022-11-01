set(LORR_VENDOR
    EASTL
    EAThread
    DirectXMath
    lw
    tracy

    spdlog
    fmt
    
    Vulkan-Headers
    imgui

    d3d12
    d3dcompiler
    dxgi
    dxguid
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