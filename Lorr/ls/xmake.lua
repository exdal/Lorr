target("ls")
    set_kind("phony")
    set_languages("cxx23")
    add_includedirs("../", { public = true })

    if is_plat("windows") then
        add_defines("LS_WINDOWS=1", { public = true })
        add_defines("_UNICODE", { force = true, public = true  })
        add_defines("UNICODE", { force = true, public = true  })
        add_defines("WIN32_LEAN_AND_MEAN", { force = true, public = true  })
        add_defines("VC_EXTRALEAN", { force = true, public = true  })
        add_defines("NOMINMAX", { force = true, public = true  })
        add_defines("_WIN32", { force = true, public = true  })
    elseif is_plat("linux") then
        add_defines("LS_LINUX=1", { public = true })
    end

    if is_mode("debug") then
        add_defines("LS_DEBUG=1", { public = true })
    end

    on_config(function (target)
        if (target:has_tool("cc", "cl")) then
            target:add("defines", "LS_COMPILER_MSVC=1", { force = true, public = true })
        elseif(target:has_tool("cc", "clang") or target:has_tool("cc", "clang_cl")) then
            target:add("defines", "LS_COMPILER_CLANG=1", { force = true, public = true })
        elseif target:has_tool("cc", "gcc") then
            target:add("defines", "LS_COMPILER_GCC=1", { force = true, public = true })
        end
    end)
target_end()
