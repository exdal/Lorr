package("vuk")
    set_homepage("https://github.com/martty")
    set_license("MIT")

    add_urls("https://github.com/exdal/vuk-xmake.git", { branch = "new-FE-dev" })

    add_versions("2024.12.25", "dbfb81ecfe948a7015aac2a3d714fc38fd37efc0")

    add_deps("function2")
    add_deps("spirv-cross")
    add_deps("fmt")

    on_install("windows|x64", "linux|x86_64", function (package)
        import("package.tools.xmake").install(package, {})
        os.cp("include/vuk", package:installdir("include"))
    end)

    on_test(function (package)
        assert(package:has_cxxincludes("function2/function2.hpp"))
    end)
package_end()
