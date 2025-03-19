add_rules("mode.release", "mode.debug")
set_project("vuk")

package("small_vector")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/gharveymn/small_vector")
    set_description("MIT")

    add_urls("https://github.com/gharveymn/small_vector.git")
    add_versions("2024.12.23", "5b4ad3bd3dc3e1593a7e95cb3843a87b5ae21000")

    add_includedirs("include", "include/stb")

    on_install(function (package)
        os.cp("source/include/gch", package:installdir("include"))
    end)
package_end()

add_requires("fmt 10.2.0", { configs = { header_only = true } })
add_requires("vulkan-memory-allocator v3.1.0")
add_requires("concurrentqueue v1.0.4")
add_requires("plf_colony v7.41")
add_requires("robin-hood-hashing 3.11.5")
add_requires("stb 2024.06.01")
add_requires("function2 4.2.4")
add_requires("spirv-cross 1.3.268+0")
add_requires("small_vector 2024.12.23")
add_requires("vk-bootstrap v1.4.307")

option("debug_allocations")
    set_default(false)
    add_defines("VUK_DEBUG_ALLOCATIONS=1")
option_end()

target("vuk")
    set_kind("static")
    add_languages("cxx23")
    add_files("src/**.cpp")
    add_includedirs("include/", { public = true })

    remove_files("src/shader_compilers/**")
    remove_files("src/extra/**")

    add_options("debug_allocations")

    add_defines("VUK_DISABLE_EXCEPTIONS", { force = true, public = true })
    -- public packages
    add_packages(
        "fmt",
        "robin-hood-hashing",
        "plf_colony",
        "function2",
        "small_vector",
        "vulkan-memory-allocator",
        "vk-bootstrap",
        { public = true })

    -- private packages
    add_packages(
        "concurrentqueue",
        "stb",
        "spirv-cross",
        { public = false })

    on_config(function (target)
        if target:has_tool("cc", "cl") then
            target:add("defines", "VUK_COMPILER_MSVC=1", { force = true, public = true })
            target:add("cxflags", "/permissive- /Zc:char8_t- /wd4068", { public = false })
        elseif target:has_tool("cc", "clang_cl") then
            target:add("defines", "VUK_COMPILER_CLANGCL=1", { force = true, public = true })
            target:add("cxflags", "-Wno-nullability-completeness", { public = false })
            target:add("cxflags", "/permissive- /Zc:char8_t- /wd4068", { public = false })
        elseif target:has_tool("cc", "clang") then
            target:add("defines", "VUK_COMPILER_CLANGPP=1", { force = true, public = true })
            target:add("cxflags", "-fno-char8_t -Wno-nullability-completeness", { public = false })
        elseif target:has_tool("cc", "gcc") then
            target:add("defines", "VUK_COMPILER_GPP=1", { force = true, public = true })
            target:add("cxflags", "-fno-char8_t", { public = false })
        end
    end)
target_end()

