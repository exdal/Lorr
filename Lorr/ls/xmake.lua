target("ls")
    set_kind("phony")
    set_languages("cxx23")
    add_includedirs("../", { public = true })

    if is_mode("debug") then
        add_defines("LS_DEBUG=1", { public = true })
    end

    if is_plat("windows") then
        add_defines("LS_WINDOWS=1", { public = true })
    elseif is_plat("linux") then
        add_defines("LS_LINUX=1", { public = true })
    end

    on_config(function (target)
        if (target:has_tool("cxx", "cl")) then
            target:add("defines", "LS_COMPILER_MSVC=1", { force = true, public = true })
        elseif(target:has_tool("cxx", "clang") or target:has_tool("cxx", "clang_cl")) then
            target:add("defines", "LS_COMPILER_CLANG=1", { force = true, public = true })
        elseif target:has_tool("cxx", "gcc") then
            target:add("defines", "LS_COMPILER_GCC=1", { force = true, public = true })
        end
    end)
target_end()