set_policy("package.precompiled", false)
add_repositories("exdal https://github.com/exdal/xmake-repo.git")

includes("xmake/*.lua")

add_rules("mode.debug", "mode.release", "mode.releasedbg", "mode.valgrind")
add_rules("plugin.compile_commands.autoupdate", { outputdir = ".", lsp = "clangd" })

set_project("Lorr")
set_version("1.0.0")

-- GLOBAL COMPILER FLAGS --
set_encodings("utf-8")
add_cxxflags("cl::/Zc:preprocessor")
add_cxxflags("clang::-fexperimental-library")

-- WARNINGS --
set_warnings("allextra", "pedantic")
add_cxxflags(
    "-Wshadow",
    "-Wno-gnu-line-marker",
    "-Wno-gnu-anonymous-struct",
    "-Wno-gnu-zero-variadic-macro-arguments",
    "-Wno-missing-braces",
    { tools = { "clang", "gcc" } })
add_cxxflags("clang::-Wshadow-all")

if is_mode("asan") then
    set_policy("build.sanitizer.address", true)
    set_policy("build.sanitizer.undefined", true)
    set_policy("build.sanitizer.leak", true)
elseif is_mode("tsan") then
    set_policy("build.sanitizer.thread", true)
    set_optimize("faster")
end

includes("Lorr")
