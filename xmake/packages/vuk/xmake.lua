package("vuk")
    set_homepage("https://github.com/martty")
    set_license("MIT")

    add_urls("https://github.com/exdal/vuk-xmake.git", { branch = "new-FE-dev" })

    add_versions("2024.12.25", "dbfb81ecfe948a7015aac2a3d714fc38fd37efc0")
    add_versions("2024.12.28", "ea8ecd70fa1d5d329724dffe003081c188410b48")
    add_versions("2024.12.30", "f29bf6562c4f4046ac57e9b5113bf10b806ba533")
    add_versions("2025.01.25", "7dd89b788d0508494edb5d1d83249ee475dddc4c")

    add_deps("function2")
    add_deps("spirv-cross")
    add_deps("fmt")

    add_configs("debug_allocations", { description = "Debug VMA allocations", default = false, type = "boolean" })

    on_install("windows|x64", "linux|x86_64", function (package)
        local configs = {}
        configs.debug_allocations = package:config("debug_allocations")
        import("package.tools.xmake").install(package, configs)
        os.cp("include/vuk", package:installdir("include"))
    end)

    on_test(function (package)
        assert(package:has_cxxincludes("function2/function2.hpp"))
    end)
package_end()
