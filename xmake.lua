add_repositories("exdal https://github.com/exdal/xmake-repo.git")
includes("xmake/**.lua")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
add_rules("plugin.compile_commands.autoupdate", { outputdir = ".", lsp = "clangd" })

set_project("Lorr")
set_version("1.0.0")
set_encodings("utf-8")

-- WARNINGS --
set_warnings("allextra", "pedantic")
add_cxxflags(
    "-Wshadow-all",
    "-Wno-gnu-line-marker",
    "-Wno-gnu-anonymous-struct",
    "-Wno-gnu-zero-variadic-macro-arguments",
    "-Wno-missing-braces",
    { tools = { "clang", "gcc" } })

-- Compiler Config --
add_ldflags("-fuse-ld=lld", { tools = { "clang" } })

includes("Lorr")
includes("tests")
