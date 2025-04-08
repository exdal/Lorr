package("ktx-software")
    set_homepage("https://github.com/KhronosGroup/KTX-Software")
    set_description("KTX (Khronos Texture) Library and Tools")
    set_license("Apache-2.0")

    set_urls("https://github.com/KhronosGroup/KTX-Software.git")

    add_versions("v4.3.2", "91ace88675ac59a97e55d0378a6602a9ae6b98bd")
    add_versions("v4.4.0", "beef80159525d9fb7abb8645ea85f4c4f6842e8f")

    add_configs("embed", {description = "Embed bitcode in binaries.", default = false, type = "boolean"})
    add_configs("ktx1", {description = "Enable KTX 1 support.", default = true, type = "boolean"})
    add_configs("ktx2", {description = "Enable KTX 2 support.", default = true, type = "boolean"})
    add_configs("shared", {description = "Build shared library.", default = false, type = "boolean", readonly = true})

    if is_plat("linux", "bsd") then
        add_syslinks("m", "pthread")
    end

    add_deps("cmake")

    on_install(function (package)
        local configs = {"-DKTX_FEATURE_TESTS=OFF", "-DKTX_LOADTEST_APPS_USE_LOCAL_DEPENDENCIES=OFF"}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        table.insert(configs, "-DKTX_FEATURE_STATIC_LIBRARY=" .. (package:config("shared") and "OFF" or "ON"))
        table.insert(configs, "-DKTX_FEATURE_TOOLS=OFF")
        table.insert(configs, "-DKTX_FEATURE_ETC_UNPACK=ON")
        table.insert(configs, "-DBASISU_SUPPORT_OPENCL=OFF")
        table.insert(configs, "-DKTX_EMBED_BITCODE=" .. (package:config("embed") and "ON" or "OFF"))
        table.insert(configs, "-DKTX_FEATURE_KTX1=" .. (package:config("ktx1") and "ON" or "OFF"))
        table.insert(configs, "-DKTX_FEATURE_KTX2=" .. (package:config("ktx2") and "ON" or "OFF"))
        table.insert(configs, "-DKTX_FEATURE_VK_UPLOAD=OFF")
        table.insert(configs, "-DKTX_FEATURE_GL_UPLOAD=OFF")
        import("package.tools.cmake").install(package, configs)

        if package:is_plat("windows", "mingw") and (not package:config("shared")) then
            package:add("defines", "KHRONOS_STATIC")
        end
    end)
package_end()
