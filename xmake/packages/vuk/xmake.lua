includes("port/packages.lua")

package("vuk")
    set_homepage("https://github.com/martty")
    set_license("MIT")

    add_urls("https://github.com/exdal/vuk.git", { branch = "set-pick" })

    add_versions("2025.02.26",   "257c1629aaa4200071fb752eb24894d05ce367d4")
    add_versions("2025.02.26.1", "bad2fe0a1e9c355bc2e9533e40d9783cef1b6f07")
    add_versions("2025.02.26.2", "9a194d2709573e4f14a2843d127db82b2cdee9ef")
    add_versions("2025.03.04",   "e6143a518dff34bebbb05b43056457dea3c86b92")
    add_versions("2025.03.07",   "add40963bb133ec42ec6a4a5d03f8c9b880a273e")
    add_versions("2025.04.08",   "8a768031c3da0a9429cfdc67bb179ec1c14a1501")
    add_versions("2025.04.08.1", "7831d3e7030ed51a3a6466120770718404449c6c")

    add_configs("debug_allocations", { description = "Debug VMA allocations", default = false, type = "boolean" })

    add_deps("spirv-cross-vuk")
    add_deps("function2")

    on_install("windows|x64", "linux|x86_64", function (package)
        local configs = {}
        configs.debug_allocations = package:config("debug_allocations")
        os.cp(path.join(os.scriptdir(), "port", "packages.lua"), "packages.lua")
        os.cp(path.join(os.scriptdir(), "port", "xmake.lua"), "xmake.lua")

        import("package.tools.xmake").install(package, configs)

        os.cp("include/vuk", package:installdir("include"))
    end)

    on_test(function (package)
        assert(package:has_cxxincludes("function2/function2.hpp"))
    end)
package_end()
