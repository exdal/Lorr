add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_project("Lorr")
set_version("1.0.0")

add_cxxflags("clang::-Wshadow-all")
set_warnings("extra")

add_requires(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "glfw",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "vk-bootstrap",
    "unordered_dense",
    "plf_colony",
    "sol2"
)
add_requires("loguru", { configs = { fmt = true } })
add_requires("slang", { configs = {
    ["enable-slang-glslang"] = true,
    ["embed-stdlib"] = true
} })

includes("Lorr")
includes("Examples")
