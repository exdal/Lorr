package("ktx-software")
    set_homepage("https://github.com/KhronosGroup/KTX-Software")
    set_description("KTX (Khronos Texture) Library and Tools")
    set_license("Apache-2.0")

    set_urls("https://github.com/KhronosGroup/KTX-Software.git")

    add_versions("v4.3.2", "91ace88675ac59a97e55d0378a6602a9ae6b98bd")

    if is_plat("linux", "bsd") then
        add_syslinks("m", "pthread")
    end

    add_deps("cmake")

    on_install(function (package)
        local configs = {
            "-DKTX_FEATURE_KTX1=ON",
            "-DKTX_FEATURE_KTX2=ON",

            "-DKTX_FEATURE_DOC=OFF",
            "-DKTX_FEATURE_JNI=OFF",
            "-DKTX_FEATURE_PY=OFF",
            "-DKTX_FEATURE_TESTS=OFF",
            "-DKTX_FEATURE_TOOLS=OFF",
            "-DKTX_FEATURE_TOOLS_CTS=OFF",
            "-DKTX_FEATURE_VK_UPLOAD=OFF",
            "-DKTX_FEATURE_GL_UPLOAD=OFF",
            "-DLIB_TYPE_DEFAULT=OFF",
            "-DBUILD_SHARED_LIBS=" .. (package:config("shared") and "ON" or "OFF"),
            "-DCMAKE_BUILD_TYPE=" .. (package:debug() and "Debug" or "Release"),
        }

        import("package.tools.cmake").install(package, configs)
    end)
package_end()
