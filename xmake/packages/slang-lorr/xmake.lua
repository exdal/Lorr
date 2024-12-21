package("slang-lorr")
    set_homepage("https://github.com/exdal/slang-vk")
    set_description("Making it easier to work with shaders")
    set_license("MIT")

    add_urls("https://github.com/exdal/slang-vk.git")

    add_versions("v2024.17.3", "de5db355eec56b4575ce4fb6c2452ab70daa17cf")

    add_configs("shared", { description = "Build shared library", default = true, type = "boolean", readonly = true })

    on_install(function (package)
        import("package.tools.xmake").install(package, {})
        os.cp("include/*.h", package:installdir("include"))
    end)
package_end()
