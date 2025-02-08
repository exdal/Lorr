package("slang-lorr")
    set_homepage("https://github.com/exdal/slang-vk")
    set_description("Making it easier to work with shaders")
    set_license("MIT")

    add_urls("https://github.com/exdal/slang-vk.git")

    add_versions("testing", "faab06dd6af464e2f9f3b0f2bbe584c7f2ba9203")
    add_versions("v2024.17.4", "8ff74412c71de16a1529f7042e9b467674ac5875")

    add_configs("shared", { description = "Build shared library", default = true, type = "boolean", readonly = true })

    on_install(function (package)
        import("package.tools.xmake").install(package, {})
        os.cp("include/*.h", package:installdir("include"))
    end)
package_end()
