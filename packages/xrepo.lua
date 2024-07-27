add_requires("fmt 10.2.1")
add_requires("xxhash")
add_requires("glm")
add_requires("vulkan-memory-allocator")
add_requires("vk-bootstrap v1.3.283")
add_requires("plf_colony")
add_requires("imgui v1.90.5-docking")
add_requires("simdutf")
add_requires("unordered_dense")
add_requires("tracy v0.10", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("loguru", { configs = { fmt = true } })
add_requires("slang-lorr v2024.1.22")
add_requires("fastgltf")
add_requires("stb 2023.12.15")
add_requires("lz4 v1.9.4")
add_requires("zstd v1.5.6")
add_requires("toml++ v3.4.0")

if is_plat("linux") then
    add_requires("glfw 3.4", { configs = {
        x11 = not has_config("wayland"),
        wayland = has_config("wayland")
    } })
else
    add_requires("glfw 3.4")
end

