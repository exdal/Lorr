package("vuk")
    set_homepage("https://github.com/martty")
    set_license("MIT")

    add_urls("https://github.com/martty/vuk.git")

    add_versions("2025.02.26",   "257c1629aaa4200071fb752eb24894d05ce367d4")
    add_versions("2025.02.26.1", "bad2fe0a1e9c355bc2e9533e40d9783cef1b6f07")
    add_versions("2025.02.26.2", "9a194d2709573e4f14a2843d127db82b2cdee9ef")
    add_versions("2025.03.04",   "e6143a518dff34bebbb05b43056457dea3c86b92")
    add_versions("2025.03.07",   "add40963bb133ec42ec6a4a5d03f8c9b880a273e")
    add_versions("2025.04.08",   "8a768031c3da0a9429cfdc67bb179ec1c14a1501")
    add_versions("2025.04.08.1", "7831d3e7030ed51a3a6466120770718404449c6c")
    add_versions("2025.04.09",   "d8b6e6462c01f7ce7520671ba1fda4f3152da2c1")
    add_versions("2025.04.14",   "b90891644b0ac8e9f77e395fc58e3c40b4091b6c")
    add_versions("2025.04.14.1", "23dbfb0b21ae426d5d2e10b2445a2f81b2257bdd")
    add_versions("2025.04.15",   "e4c5e487b25cb98dcc0234653ca986f8504444ee")
    add_versions("2025.04.19",   "75771a95ca380af9323eaffd62186ce957c793ec")
    add_versions("2025.04.22",   "4b9918436e48b91fc89164b54fbb3cbafb6331de")
    add_versions("2025.04.28",   "ceded9151342919a12a61617fc1fd5dcca99e0e4")
    add_versions("2025.04.29",   "024df778cfcb79d21fc63236aa5427fcb3823acf")
    add_versions("2025.05.06",   "73fee60d5f23bf1f14a7b1f6d8b66e19d74b956b")
    add_versions("2025.06.15",   "ab3bf6c51e31bdb3eb51f85845b83f939d4132de")
    add_versions("2025.07.09",   "8a1b873f7d0e4bb36ecd680a608a1e057655bb8c")
    add_versions("2025.09.01",   "c9d2aeea71fe6cd41bd10ea47c38a390546b66ab")

    add_configs("debug_allocations", { description = "Debug VMA allocations", default = false, type = "boolean" })
    add_configs("disable_exceptions", { description = "Disalbe exceptions", default = false, type = "boolean" })

    add_deps("spirv-cross 1.4.309+0")
    add_deps("function2")

    on_load(function (package)
        if package:config("disable_exceptions") then
            package:add("defines", "VUK_DISABLE_EXCEPTIONS=1")
        end

        if package:config("debug_allocations") then
            package:add("defines", "VUK_DEBUG_ALLOCATIONS=1")
        end
    end)

    on_install("windows|x64", "linux|x86_64", function (package)
        local configs = {}
        configs.debug_allocations = package:config("debug_allocations")
        configs.disable_exceptions = package:config("disable_exceptions")
        os.cp(path.join(os.scriptdir(), "port", "xmake.lua"), "xmake.lua")

        import("package.tools.xmake").install(package, configs)

        os.cp("include/vuk", package:installdir("include"))
    end)

    on_test(function (package)
        assert(package:has_cxxincludes("function2/function2.hpp"))
    end)
package_end()

