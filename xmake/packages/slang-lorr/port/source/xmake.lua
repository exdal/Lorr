includes("packages.lua")
add_requires("unordered_dense v4.5.0")
add_requires("miniz 2.2.0")
add_requires("lz4 v1.10.0")
add_requires("slang-spirv-headers sync")
add_requires("slang-glslang-compiler sync")
add_requires("slang-spirv-tools sync")

includes("slang_target.lua")
includes("core")
includes("compiler-core")
includes("slang-core-module")
includes("slang")
includes("slang-glslang")

add_slang_target("slang-build-all", {
    kind = "phony",
    default = true,
    fence = true,
    deps = {
        { "slang", "slang-glslang" }
    },
})
