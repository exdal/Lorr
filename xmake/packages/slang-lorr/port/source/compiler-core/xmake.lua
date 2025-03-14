add_slang_target("compiler-core", {
    includes = {
        { ".", { public = true } }
    },
    files = {
        { "./*.cpp" }
    },
    deps = {
        { "core", { public = false } }
    },
    windows_files = "./windows/*.cpp",
    packages = {
        { "slang-spirv-headers", { public = true } }
    }
})


