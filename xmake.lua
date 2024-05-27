add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_project("Lorr")
set_version("1.0.0")

add_cxxflags("clang::-Wshadow-all")
set_warnings("extra")
set_runtimes("MT", "c++_static")

option("profile")
    set_default(false)
    set_description("Enable application wide profiling.")
option_end()

if has_config("profile") then
    add_defines("TRACY_ENABLE=1", { public = true })
    if is_plat("windows") then
        add_defines("TRACY_IMPORTS")
    end
end

add_requires(
    "fmt",
    "xxhash",
    "glm",
    "glfw 3.4",
    "vulkan-memory-allocator",
    "vk-bootstrap v1.3.283",
    "plf_colony",
    "sol2",
    "imgui v1.90.5-docking",
    "simdutf",
    "unordered_dense"
)
add_requires("tracy 005d0929035bdc13f877da97c6631c0f2c98673e", { configs = {
    on_demand = true,
    callstack = true,
    callstack_inlines = false,
    code_transfer = true,
    exit = true,
    system_tracing = true,
} })
add_requires("loguru", { configs = { fmt = true } })
add_requires("slang", { configs = {
    slang_glslang = true,
    embed_stdlib = true
} })

includes("Lorr")
includes("Examples")
