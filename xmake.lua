set_project("Lorr")
set_version("1.0.0")

if is_mode("debug") then
  set_optimize("none")
elseif is_mode("release") then
  set_optimize("aggressive")
end

add_requires("slang")

add_requires(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "vk-bootstrap",
    "unordered_dense",
    "plf_colony",
    "sol2"
)

includes("Lorr")
includes("Examples")
