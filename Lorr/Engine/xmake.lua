target("Lorr")
  set_kind("static")
  set_languages("cxx20")
  add_forceincludes("pch.hh", { public = true })
  set_pcheader("pch.hh", { public = true })
  add_cxxflags("clang::-march=native", "clang_cl::/arch:AVX2")

  add_includedirs("./", { public = true })
  add_files("**.cc")
  add_rpathdirs("@executable_path")

  add_defines("LR_PROJECT_DIR=\"$(projectdir)\"", { public = true })
  if is_mode("debug") then
    add_defines("LR_DEBUG", { public = true })
  end

  if is_os("windows") then
    add_defines("LR_WIN32=1", { public = true })
    add_syslinks("gdi32", "msimg32", "user32")

    remove_files("Window/X11*")
    remove_files("OS/Linux*")
  elseif is_os("linux") then
    add_defines("LR_LINUX=1", { public = true })
    add_syslinks("xcb")

    remove_files("Window/Win32*")
    remove_files("OS/Win32*")
  end

  add_packages("slang", { public = true })

  add_packages(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "glfw",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "vk-bootstrap",
    "compiler-core",
    "unordered_dense",
    "plf_colony",
    "sol2",
    "loguru",
    { public = true })

target_end()
