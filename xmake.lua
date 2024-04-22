set_project("Lorr")
set_version("1.0.0")

if is_mode("debug") then
  set_optimize("none")
  set_symbols("debug")
elseif is_mode("release") then
  set_optimize("aggressive")
end

add_requires(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "glfw",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "vk-bootstrap",
    "slang",
    "unordered_dense",
    "plf_colony",
    "sol2"
)
add_requires("loguru", { configs = { fmt = true } })

includes("Lorr")
includes("Examples")
