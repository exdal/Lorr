add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_project("Lorr")
set_version("1.0.0")
set_runtimes("MT", "c++_static")

-- WARNINGS --
add_cxxflags(
    "-Wshadow-all",
    "-Wno-gnu-line-marker",
    "-Wno-gnu-anonymous-struct",
    "-Wno-gnu-zero-variadic-macro-arguments",
    "-Wno-missing-braces",
    { tools = { "clang", "gcc" } })
set_warnings("allextra", "pedantic")

-- OPTIONS --
option("profile")
    set_default(false)
    set_description("Enable application wide profiling.")
    add_defines("TRACY_ENABLE=1", { public = true })
    if is_plat("windows") then
        add_defines("TRACY_IMPORTS=1", { public = true })
    end
option_end()

option("wayland")
    set_default(false)
    set_description("Enable wayland windowing for Linux.")
    add_defines("LR_WAYLAND=1", { public = true })
option_end()

option("build_examples")
    set_default(false)
    set_description("Build examples")
option_end()

-- PACKAGES --
add_requires("fmt 10.2.1")
add_requires("xxhash")
add_requires("glm")
add_requires("vulkan-memory-allocator")
add_requires("vk-bootstrap v1.3.283")
add_requires("plf_colony")
add_requires("sol2")
add_requires("imgui v1.90.5-docking")
add_requires("simdutf")
add_requires("unordered_dense")
add_requires("tracy", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("loguru", { configs = { fmt = true } })
add_requires("slang v2024.1.18", { configs = {
    slang_glslang = true,
    embed_stdlib = true
} })

if is_plat("linux") then
    add_requires("glfw 3.4", { configs = {
        x11 = not has_config("wayland"),
        wayland = has_config("wayland")
    } })
else
    add_requires("glfw 3.4")
end

includes("Lorr")
includes("Examples")
