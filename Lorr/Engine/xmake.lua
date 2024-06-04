target("Lorr")
  set_kind("static")
  set_languages("cxx20")
  add_forceincludes("Engine/pch.hh", { public = true })
  set_pcheader("Engine/pch.hh", { public = true })
  add_cxxflags("clang::-march=native", "clang_cl::/arch:AVX2", "cl::/arch:AVX2")
  set_runtimes("MT", "c++_static")

  add_includedirs("../", { public = true });
  add_files("**.cc")
  add_rpathdirs("@executable_path")

  add_options("profile")
  add_options("wayland")

-- Embedded resources --
  add_files("../Resources/shaders/**.slang", { rules = "utils.bin2c" })

  if is_mode("debug") then
    add_defines("LR_DEBUG", { public = true })
  end

  if is_os("windows") then
    add_defines("LR_WIN32=1", { public = true })
    add_syslinks("gdi32", "msimg32", "user32")
    remove_files("OS/Linux*")
  elseif is_os("linux") then
    add_defines("LR_LINUX=1", { public = true })
    if has_config("wayland") then
    else
        add_syslinks("xcb")
    end
    remove_files("OS/Win32*")
  end

  add_packages(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "glfw",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "vk-bootstrap",
    "plf_colony",
    "sol2",
    "loguru",
    "imgui",
    "simdutf",
    "slang",
    "unordered_dense",
    { public = true })

    on_load(function (target)
        local headerdir = path.join(target:autogendir(), "rules", "utils", "bin2c")
        target:add("includedirs", headerdir, { public = true })
    end)

target_end()
