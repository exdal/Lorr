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
    add_versions("1.4.309+0", "2c32b6bf86f3c4a5539aa1f0bacbd59fe61759cf")

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

