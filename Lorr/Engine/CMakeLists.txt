set(LORR_VENDOR
    EASTL
    EAThread
    DirectXMath
    lw
    tracy

    fmt

    Vulkan-Headers
    imgui

    d3d12
    dxcompiler
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
    INCLUDE_PUB
        ./
        ../../
    LIBTYPE
        STATIC
)