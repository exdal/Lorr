target("Lorr")
  set_kind("static")
  set_languages("cxx20")
  set_warnings("extra")
  add_forceincludes("pch.hh", { public = true })
  set_pcheader("pch.hh", { public = true })
  add_syslinks("gdi32", "msimg32", "user32")

  add_cxxflags("clang::-march=native", "clang_cl::/arch:AVX2")
  add_includedirs("./", { public = true })
  add_files("**.cc")

  add_packages("slang", { public = true })

  add_packages(
    "fmt",
    "tracy",
    "xxhash",
    "glm",
    "vulkan-headers",
    "vulkan-memory-allocator",
    "vk-bootstrap",
    "compiler-core",
    "unordered_dense",
    "plf_colony",
    "sol2",
    { public = true })

target_end()
