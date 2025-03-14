add_repositories("exdal https://github.com/exdal/xmake-repo.git")

includes("packages/ImGuizmo/xmake.lua")
includes("packages/slang-lorr/xmake.lua")
includes("packages/vuk/xmake.lua")

add_requires("xxhash v0.8.3")
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}})
add_requires("plf_colony v7.41")
add_requires("imgui v1.91.6-docking", { configs = {
    wchar32 = true,
} })
add_requires("simdutf v6.2.0")
add_requires("simdjson v3.11.3")
add_requires("unordered_dense v4.5.0")
add_requires("tracy v0.11.1", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("vk-bootstrap v1.4.307")
add_requires("fastgltf v0.8.0")
add_requires("stb 2024.06.01")
add_requires("lz4 v1.10.0")
add_requires("zstd v1.5.6")
add_requires("flecs v4.0.3")

add_requires("libsdl3 3.2.2", { configs = {
    wayland = true,
    x11 = true,
} })

add_requires("imguizmo-lorr 1.89+WIP")
add_requires("slang-lorr v2025.6.1")
add_requires("vuk 2025.03.07", { configs = {
    debug_allocations = false
}, debug = is_mode("debug") })

add_requires("meshoptimizer v0.22")
