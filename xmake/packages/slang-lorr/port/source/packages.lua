package("slang-spirv-headers")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/KhronosGroup/SPIRV-Headers/")
    set_description("SPIR-V Headers")
    set_license("MIT")

    add_urls("https://github.com/KhronosGroup/SPIRV-Headers.git")
    add_versions("sync", "e7294a8ebed84f8c5bd3686c68dbe12a4e65b644")

    add_deps("cmake")

    on_install(function (package)
        import("package.tools.cmake").install(package, {
            "-DSPIRV_HEADERS_SKIP_EXAMPLES=ON",
            "-DSPIRV_HEADERS_ENABLE_TESTS=OFF"
        })
    end)

    on_test(function (package)
        assert(package:check_csnippets({test = [[
            void test() {
                int version = SPV_VERSION;
            }
        ]]}, {includes = "spirv/unified1/spirv.h"}))
    end)
package_end()

package("slang-spirv-tools")
    set_homepage("https://github.com/KhronosGroup/SPIRV-Tools/")
    set_description("SPIR-V Tools")
    set_license("Apache-2.0")

    add_urls("https://github.com/KhronosGroup/SPIRV-Tools.git")
    add_versions("sync", "3364b982713a0440d1d342dd5eec65b122a61b71")

    add_deps("slang-spirv-headers sync")
    add_deps("cmake >=3.17.2")
    --add_deps("python 3.x", { kind = "binary" })

    on_install(function (package)
        package:addenv("PATH", "bin")
        local configs = {"-DSPIRV_SKIP_TESTS=ON", "-DSPIRV_WERROR=OFF"}
        -- walkaround for potential conflict with parallel build & debug pdb generation
        if package:debug() then
            table.insert(configs, "-DCMAKE_COMPILE_PDB_OUTPUT_DIRECTORY=''")
        end
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        local spirv = package:dep("slang-spirv-headers")
        table.insert(configs, "-DSPIRV-Headers_SOURCE_DIR=" .. spirv:installdir():gsub("\\", "/"))
        if package:is_plat("windows") then
            import("package.tools.cmake").install(package, configs, {buildir = os.tmpfile() .. ".dir"})
        else
            import("package.tools.cmake").install(package, configs)
        end
        package:add("links", "SPIRV-Tools-link", "SPIRV-Tools-reduce", "SPIRV-Tools-opt")
        if package:config("shared") then
            package:add("links", "SPIRV-Tools-shared")
        else
            package:add("links", "SPIRV-Tools")
        end
    end)

    on_test(function (package)
        if not package:is_cross() then
            os.runv("spirv-as --help")
            os.runv("spirv-opt --help")
        end
        assert(package:has_cxxfuncs("spvContextCreate", {configs = {languages = "c++17"}, includes = "spirv-tools/libspirv.hpp"}))
    end)
package_end()

package("slang-glslang")
    set_homepage("https://github.com/KhronosGroup/glslang/")
    set_description("Khronos-reference front end for GLSL/ESSL, partial front end for HLSL, and a SPIR-V generator.")
    set_license("Apache-2.0")

    add_urls("https://github.com/KhronosGroup/glslang.git")
    add_versions("sync", "a0995c49ebcaca2c6d3b03efbabf74f3843decdb")

    add_configs("binaryonly", {description = "Only use binary program.", default = false, type = "boolean"})
    add_configs("exceptions", {description = "Build with exception support.", default = false, type = "boolean"})
    add_configs("rtti",       {description = "Build with RTTI support.", default = false, type = "boolean"})
    add_configs("default_resource_limits",       {description = "Build with default resource limits.", default = false, type = "boolean"})
    if is_plat("wasm") then
        add_configs("shared", {description = "Build shared library.", default = false, type = "boolean", readonly = true})
    end

    add_deps("cmake")
    add_deps("slang-spirv-tools sync")
    if is_plat("linux") then
        add_syslinks("pthread")
    end

    add_defines("ENABLE_HLSL")

    on_load(function (package)
        if package:config("binaryonly") then
            package:set("kind", "binary")
        end
    end)

    on_fetch(function (package, opt)
        if opt.system and package:config("binaryonly") then
            return package:find_tool("glslangValidator")
        end
    end)

    on_install(function (package)
        package:addenv("PATH", "bin")
        io.replace("CMakeLists.txt", "ENABLE_OPT OFF", "ENABLE_OPT ON")
        io.replace("StandAlone/CMakeLists.txt", "target_link_libraries(glslangValidator ${LIBRARIES})", [[
            target_link_libraries(glslangValidator ${LIBRARIES} SPIRV-Tools-opt SPIRV-Tools-link SPIRV-Tools-reduce SPIRV-Tools)
        ]], {plain = true})
        io.replace("SPIRV/CMakeLists.txt", "target_link_libraries(SPIRV PRIVATE MachineIndependent SPIRV-Tools-opt)", [[
            target_link_libraries(SPIRV PRIVATE MachineIndependent SPIRV-Tools-opt SPIRV-Tools-link SPIRV-Tools-reduce SPIRV-Tools)
        ]], {plain = true})
        -- glslang will add a debug lib postfix for win32 platform, disable this to fix compilation issues under windows
        io.replace("CMakeLists.txt", 'set(CMAKE_DEBUG_POSTFIX "d")', [[
            message(WARNING "Disabled CMake Debug Postfix for xmake package generation")
        ]], {plain = true})
        local configs = {"-DENABLE_CTEST=OFF", "-DBUILD_EXTERNAL=OFF"}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"))
        if package:is_plat("windows") then
            table.insert(configs, "-DBUILD_SHARED_LIBS=OFF")
            if package:debug() then
                table.insert(configs, "-DCMAKE_COMPILE_PDB_OUTPUT_DIRECTORY=''")
            end
        else
            table.insert(configs, "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"))
        end
        table.insert(configs, "-DENABLE_EXCEPTIONS=" .. (package:config("exceptions") and "ON" or "OFF"))
        table.insert(configs, "-DENABLE_RTTI=" .. (package:config("rtti") and "ON" or "OFF"))
        table.insert(configs, "-DALLOW_EXTERNAL_SPIRV_TOOLS=ON")
        import("package.tools.cmake").install(package, configs, {packagedeps = {"slang-spirv-tools sync"}})
        if not package:config("binaryonly") then
            package:add("links", "glslang", "MachineIndependent", "GenericCodeGen", "OGLCompiler", "OSDependent", "HLSL", "SPIRV", "SPVRemapper")
        end
        if package:config("default_resource_limits") then
            package:add("links", "glslang", "glslang-default-resource-limits")
        end

        -- https://github.com/KhronosGroup/glslang/releases/tag/12.3.0
        local bindir = package:installdir("bin")
        local glslangValidator = path.join(bindir, "glslangValidator" .. (is_host("windows") and ".exe" or ""))
        if not os.isfile(glslangValidator) then
            local glslang = path.join(bindir, "glslang" .. (is_host("windows") and ".exe" or ""))
            os.trycp(glslang, glslangValidator)
        end
    end)

    on_test(function (package)
        if not package:is_cross() then
            os.vrun("glslangValidator --version")
        end

        if not package:config("binaryonly") then
            assert(package:has_cxxfuncs("ShInitialize", {configs = {languages = "c++11"}, includes = "glslang/Public/ShaderLang.h"}))
        end
    end)
package_end()

