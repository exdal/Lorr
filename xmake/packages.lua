add_requires("fmt 11.1.4", { configs = {
    header_only = true
}, system = false })

add_requires("xxhash v0.8.3", { system = false })
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}, system = false })
add_requires("plf_colony v7.41")

add_requires("imguizmo v1.91.8-docking")
add_requires("imgui v1.91.8-docking", { configs = {
    wchar32 = true,
}, system = false })

add_requires("simdutf v6.2.0", { system = false })
add_requires("simdjson v3.12.2", { system = false })
add_requires("unordered_dense v4.5.0", { system = false })
add_requires("tracy v0.11.1", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
}, system = false })
add_requires("vk-bootstrap v1.4.307", { system = false, debug  = is_mode("debug") })
add_requires("fastgltf v0.8.0", { system = false, debug = is_mode("debug") })
add_requires("stb 2024.06.01", { system = false })
add_requires("lz4 v1.10.0", { system = false })
add_requires("zstd v1.5.6", { system = false })
add_requires("flecs v4.0.4", { system = false })

add_requires("libsdl3 3.2.10", { configs = {
    wayland = false,
    x11 = true,
}, system = false, debug  = is_mode("debug") })

add_requires("shader-slang v2025.6.3", { system = false })
add_requires("vuk 2025.04.29", { configs = {
    debug_allocations = false,
    disable_exceptions = true,
}, debug = is_mode("debug") })

add_requires("meshoptimizer v0.22", { system = false })
add_requires("ktx-software v4.4.0", { system = false })
