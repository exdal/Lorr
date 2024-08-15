add_requires("fmt 11.0.2")
add_requires("xxhash")
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}})
add_requires("vulkan-memory-allocator")
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
add_requires("vk-bootstrap v1.3.292")
add_requires("fastgltf v0.8.0")
add_requires("stb 2023.12.15")
add_requires("lz4 v1.9.4")
add_requires("zstd v1.5.6")

if is_plat("linux") then
    add_requires("glfw 3.4", { configs = {
        x11 = not has_config("wayland"),
        wayland = has_config("wayland")
    } })
else
    add_requires("glfw 3.4")
end

