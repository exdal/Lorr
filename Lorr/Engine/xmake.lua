target("Lorr")
    set_kind("static")
    set_languages("cxx23")
    add_forceincludes("Engine/pch.hh", { public = true })
    set_pcheader("Engine/pch.hh", { public = true })
    add_cxxflags("clang::-march=native", "clang_cl::/arch:AVX2", "cl::/arch:AVX2")
    set_runtimes("MT", "c++_static")

    if is_mode("debug") then
        add_ldflags(
            "-Wl,--export-dynamic",
            { tools = { "clang", "gcc" } })
    end

    add_includedirs("../", { public = true });
    add_files("**.cc")
    add_rpathdirs("@executable_path")

    add_options("profile")
    add_options("wayland")

    -- Embedded resources --
    add_files("../Resources/embedded/**", { rules = "utils.bin2c" })

    if is_mode("debug") then
        add_defines("LR_DEBUG", { public = true })
    end

    if is_plat("windows") then
        add_defines("LR_WIN32=1", { public = true })
        add_syslinks("gdi32", "msimg32", "user32")
        remove_files("OS/Linux*")
    elseif is_plat("linux") then
        add_defines("LR_LINUX=1", { public = true })
        if not has_config("wayland") then
            add_syslinks("xcb")
        end
        remove_files("OS/Win32*")
    end

    add_packages(
        "glfw",
        "vulkan-headers",
        "vulkan-memory-allocator",
        "vk-bootstrap",
        "imgui",
        "slang-lorr",
        "fmt",
        "tracy",
        "xxhash",
        "glm",
        "plf_colony",
        "loguru",
        "simdutf",
        "unordered_dense",
        "lz4",
        "zstd",
        "fastgltf",
        "stb",
        { public = true })

    on_config(function (target)
        local gendir = target:autogendir()
        local headerdir = path.join(target:autogendir(), "rules", "utils", "bin2c")
        target:add("includedirs", headerdir, { public = true })
        target:add("includedirs", gendir, { public = true })
        target:set("configdir", gendir)
        local embedded_str = ""
        for _, file_path in ipairs(os.files("$(projectdir)/Lorr/Resources/embedded/**")) do
            local file_name_full = file_path:match("([^/\\]+)$")
            local file_name = file_name_full:gsub("%..-$", "")

            local comment_str = string.format("// %s\n", file_path)
            local arr_str = string.format("constexpr static _data_t %s[] = {\n#include <%s.h>\n};\n", file_name, file_name_full)
            local span_str = string.format("constexpr static ls::span %s_sp = { &%s->as_u8, count_of(%s) - 1 };\n", file_name, file_name, file_name)
            local sv_str = string.format("constexpr static std::string_view %s_str = { &%s->as_c8, count_of(%s) -1 };\n", file_name, file_name, file_name)
            embedded_str = embedded_str .. comment_str .. arr_str .. span_str .. sv_str .. '\n'
        end

        target:set("configvar", "LR_EMBEDDED_INCLUDES", embedded_str)
        target:add("configfiles", "$(projectdir)/Lorr/Resources/Embedded.hh.in")
    end)

target_end()
