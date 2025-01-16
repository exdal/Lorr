add_requires("xxhash v0.8.2")
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}})
add_requires("plf_colony v7.41")
add_requires("imgui v1.91.6-docking")
add_requires("simdutf v5.5.0")
add_requires("simdjson v3.10.0")
add_requires("unordered_dense v4.4.0")
add_requires("tracy v0.11.1", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("vk-bootstrap v1.3.292")
add_requires("fastgltf v0.8.0")
add_requires("stb 2023.12.15")
add_requires("lz4 v1.9.4")
add_requires("zstd v1.5.6")
add_requires("flecs v4.0.3")

add_requires("libsdl 2.30.10", { configs = {
    sdlmain = false,
    x11 = false,
} })

add_requires("imguizmo-lorr 1.89+WIP")
add_requires("slang-lorr v2024.17.4")
add_requires("vuk 2024.12.30", { configs = {
    debug_allocations = false
} })

add_requires("meshoptimizer v0.22")
