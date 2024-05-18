add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_project("Lorr")
set_version("1.0.0")

add_cxxflags("clang::-Wshadow-all")
set_warnings("extra")
set_runtimes("MT", "c++_static")

add_requires(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "glfw",
    "vulkan-memory-allocator",
    "vk-bootstrap v1.3.283",
    "plf_colony",
    "sol2",
    "imgui v1.90.5-docking",
    "simdutf",
    "unordered_dense"
)
add_requires("loguru", { configs = { fmt = true } })
add_requires("slang", { configs = {
    ["enable-slang-glslang"] = true,
    ["embed-stdlib"] = true
} })

includes("Lorr")
includes("Examples")
