package("fmtlog")
    set_homepage("https://github.com/MengRao/fmtlog")
    set_description("fmtlog is a performant fmtlib-style logging library with latency in nanoseconds.")
    set_license("MIT")

    add_urls("https://github.com/MengRao/fmtlog/archive/refs/tags/$(version).tar.gz",
             "https://github.com/MengRao/fmtlog.git", {submodules = false})

    add_versions("v2.3.0", "769dee37a6375e2c4784c936c7191aaa755e669ef9ed311c412153305878ba56")

    add_deps("cmake")
    add_deps("fmt")

    if is_plat("linux") then
        add_syslinks("pthread")
    end

    on_install("linux", "macosx", "windows|!arm64", function (package)
        os.cp(path.join(os.scriptdir(), "port", "xmake.lua"), "xmake.lua")

        local configs = {}
        import("package.tools.xmake").install(package, configs)

        if package:config("shared") then
            os.tryrm(path.join(package:installdir("lib"),  "*.a"))
        else
            os.tryrm(path.join(package:installdir("lib"),  "*.dll"))
            os.tryrm(path.join(package:installdir("lib"),  "*.dylib"))
            os.tryrm(path.join(package:installdir("lib"),  "*.so"))
        end
        os.cp("*.h", package:installdir("include/fmtlog"))
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({test = [[
            void test() {
                logi("A info msg");
            }
        ]]}, {configs = {languages = "c++17"}, includes = "fmtlog/fmtlog.h"}))
    end)

