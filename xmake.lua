includes("xmake/*.lua")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.compile_commands.autoupdate", { outputdir = ".", lsp = "clangd" })

set_project("Lorr")
set_version("1.0.0")

-- GLOBAL COMPILE FLAGS --
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

includes("Lorr")
