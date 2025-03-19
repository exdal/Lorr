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

package("spirv-cross-vuk")
    set_homepage("https://github.com/KhronosGroup/SPIRV-Cross/")
    set_description("SPIRV-Cross is a practical tool and library for performing reflection on SPIR-V and disassembling SPIR-V back to high level languages.")
    set_license("Apache-2.0")

    add_urls("https://github.com/KhronosGroup/SPIRV-Cross.git")
    add_versions("1.2.154+1", "e6f5ce6b8998f551f3400ad743b77be51bbe3019")
    add_versions("1.2.162+0", "6d10da0224bd3214c9a507832e62d9fb6ae9620d")
    add_versions("1.2.189+1", "0e2880ab990e79ce6cc8c79c219feda42d98b1e8")
    add_versions("1.3.231+1", "f09ba2777714871bddb70d049878af34b94fa54d")
    add_versions("1.3.268+0", "2de1265fca722929785d9acdec4ab728c47a0254")

    add_configs("exceptions", {description = "Enable exception handling", default = true, type = "boolean"})

    add_deps("cmake")

    if is_plat("windows") then
        set_policy("platform.longpaths", true)
    end

    on_load(function (package)
        local links = { "spirv-cross-core" }
        for _, link in ipairs(links) do
            if package:is_plat("windows") and package:is_debug() then
                link = link .. "d"
            end
            package:add("links", link)
        end
    end)

    on_install("windows", "linux", "macosx", "mingw", function (package)
        local configs = {
            "-DSPIRV_CROSS_ENABLE_TESTS=OFF",
            "-DSPIRV_CROSS_CLI=OFF",
            "-DSPIRV_CROSS_ENABLE_HLSL=OFF",
            "-DSPIRV_CROSS_ENABLE_MSL=OFF",
            "-DSPIRV_CROSS_ENABLE_CPP=OFF",
            "-DSPIRV_CROSS_ENABLE_REFLECT=OFF",
            "-DSPIRV_CROSS_ENABLE_C_API=OFF",
            "-DSPIRV_CROSS_ENABLE_UTIL=OFF"
        }
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))

        local cxflags
        if package:config("exceptions") then
            table.insert(configs, "-DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=OFF")
            if package:is_plat("windows") and package:has_tool("cxx", "cl", "clang_cl") then
                cxflags = {"/EHsc"}
            end
        else
            table.insert(configs, "-DSPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS=ON")
        end
        if package:is_plat("windows") and package:is_debug() then
            cxflags = cxflags or {}
            table.insert(cxflags, "/FS")
        end
        if package:config("shared") then
            table.insert(configs, "-DSPIRV_CROSS_SHARED=ON")
        else
            table.insert(configs, "-DSPIRV_CROSS_SHARED=OFF")
        end

        import("package.tools.cmake").install(package, configs, {cxflags = cxflags})

        os.cp("*.hpp", package:installdir("include"))
        package:addenv("PATH", "bin")
    end)
package_end()

add_requires("fmt 11.1.4", { configs = { header_only = true } })
add_requires("vulkan-memory-allocator v3.1.0")
add_requires("concurrentqueue v1.0.4")
add_requires("plf_colony v7.41")
add_requires("robin-hood-hashing 3.11.5")
add_requires("stb 2024.06.01")
add_requires("function2 4.2.4")
add_requires("spirv-cross-vuk 1.3.268+0")
add_requires("small_vector 2024.12.23")
add_requires("vk-bootstrap v1.4.307")

option("debug_allocations")
    set_default(false)
    add_defines("VUK_DEBUG_ALLOCATIONS=1")
option_end()

target("vuk")
    set_kind("static")
    add_languages("cxx20")
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
        "spirv-cross-vuk",
        { public = false })

    if is_os("windows") then
        add_defines(
            "NOMINMAX",
            "VC_EXTRALEAN",
            "WIN32_LEAN_AND_MEAN",
            "_CRT_SECURE_NO_WARNINGS",
            "_SCL_SECURE_NO_WARNINGS",
            "_SILENCE_CLANG_CONCEPTS_MESSAGE",
            "_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING",
            { public = true })
    end

    on_config(function (target)
        if target:has_tool("cc", "cl") then
            target:add("defines", "VUK_COMPILER_MSVC=1", { force = true, public = true })
            target:add("cxflags", "/GR /Zi /RTC1 /permissive- /Zc:char8_t- /wd4068", { public = false })
        elseif target:has_tool("cc", "clang_cl") then
            target:add("defines", "VUK_COMPILER_CLANGCL=1", { force = true, public = true })
            target:add("cxflags", "-Wno-nullability-completeness", { public = false })
            target:add("cxflags", "/GR /Zi /RTC1 /permissive- /Zc:char8_t- /wd4068", { public = false })
        elseif target:has_tool("cc", "clang") then
            target:add("defines", "VUK_COMPILER_CLANGPP=1", { force = true, public = true })
            target:add("cxflags", "-fno-char8_t -Wno-nullability-completeness", { public = false })
        elseif target:has_tool("cc", "gcc") then
            target:add("defines", "VUK_COMPILER_GPP=1", { force = true, public = true })
            target:add("cxflags", "-fno-char8_t", { public = false })
        end
    end)
target_end()

