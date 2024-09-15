add_requires("fmt 11.0.2")
add_requires("xxhash v0.8.2")
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}})
add_requires("vulkan-memory-allocator v3.1.0")
add_requires("plf_colony v7.41")
add_requires("imgui v1.91.0-docking")
add_requires("simdutf v5.3.11")
add_requires("glaze v3.1.7")
add_requires("unordered_dense v4.4.0")
add_requires("tracy v0.10", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("loguru v2.1.0", { configs = { fmt = true } })
add_requires("slang-lorr v2024.9.1")
add_requires("vk-bootstrap v1.3.292")
add_requires("fastgltf v0.8.0")
add_requires("stb 2023.12.15")
add_requires("lz4 v1.9.4")
add_requires("zstd v1.5.6")
add_requires("flecs v4.0.0")

if is_plat("linux") then
    add_requires("glfw 3.4", { configs = {
        x11 = not has_config("wayland"),
        wayland = has_config("wayland")
    } })
else
    add_requires("glfw 3.4")
end

if has_config("enable_tests") then
    add_requires("gtest v1.15.2", { configs = { main = true, gmock = true } })
end

add_requires("imguizmo-lorr 1.89+WIP")
