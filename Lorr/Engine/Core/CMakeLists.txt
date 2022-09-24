set(ENGINE_VENDOR
    EASTL
    EAThread
    DirectXMath
    lw
    fmt
    spdlog
    tracy
    
    Vulkan-Headers

    d3dcompiler
    dxgi
    dxguid
    d3d12
)

set(ENGINE_COMMON
    LCore-Graphics
    LCore-IO
    LCore-Logger
    LCore-OS
    LCore-Window
)

######### Engine components ########
# Graphics
LorrComponent(
    NAME
        LCore-Graphics
    SOURCE
        Graphics/*.cc
    VENDOR
        ${ENGINE_VENDOR}
    PCH
        pch.hh
    INCLUDE
        ./
    LIBTYPE
        STATIC
)

# IO
LorrComponent(
    NAME
        LCore-IO
    SOURCE
        IO/*.cc
    VENDOR
        ${ENGINE_VENDOR}
    PCH
        pch.hh
    INCLUDE
        ./
    LIBTYPE
        STATIC
)

# Logger
LorrComponent(
    NAME
        LCore-Logger
    SOURCE
        Logger/*.cc
    VENDOR
        ${ENGINE_VENDOR}
    PCH
        pch.hh
    INCLUDE
        ./
    LIBTYPE
        STATIC
)

# OS
LorrComponent(
    NAME
        LCore-OS
    SOURCE
        OS/*.cc
    VENDOR
        ${ENGINE_VENDOR}
    PCH
        pch.hh
    INCLUDE
        ./
    LIBTYPE
        STATIC
)

# Window
LorrComponent(
    NAME
        LCore-Window
    SOURCE
        Window/*.cc
    VENDOR
        ${ENGINE_VENDOR}
    PCH
        pch.hh
    INCLUDE
        ./
    LIBTYPE
        STATIC
)

# Core
LorrComponent(
    NAME
        LCore
    SOURCE
        Engine.cc pch.cc
    VENDOR
        ${ENGINE_COMMON}
    PCH
        pch.hh
    INCLUDE
        ./
    LIBTYPE
        STATIC
)