add_requires("fmt 11.1.4", { configs = {
    header_only = true
} })
add_requires("fmtlog v2.3.0")

add_requires("xxhash v0.8.3")
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
} })
add_requires("plf_colony v7.41")

add_requires("imguizmo-lr v1.91.8-docking")
add_requires("imgui v1.91.8-docking", { configs = {
    wchar32 = true,
} })

add_requires("simdutf v6.2.0")
add_requires("simdjson v3.12.2")
add_requires("unordered_dense v4.5.0")
add_requires("tracy v0.11.1", { configs = {
    tracy_enable = false,
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("vk-bootstrap v1.4.307", { system = false, debug  = is_mode("debug") })
add_requires("fastgltf v0.8.0")
add_requires("stb 2024.06.01")
add_requires("lz4 v1.10.0")
add_requires("zstd v1.5.6")
add_requires("flecs v4.0.4")

add_requires("libsdl3")

add_requires("shader-slang v2025.12.1")
add_requires("vuk 2025.06.15", { configs = {
    debug_allocations = false,
    disable_exceptions = true,
}, debug = is_mode("debug") })

add_requires("meshoptimizer v0.22")
add_requires("ktx v4.4.0")
