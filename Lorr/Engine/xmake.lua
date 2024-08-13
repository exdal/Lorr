rule("lorr.install_resources")
    set_extensions(".ttf", ".slang", ".lua")
    before_buildcmd_file(function (target, batchcmds, sourcefile, opt)
        local output_dir = target:extraconf("rules", "lorr.install_resources", "output_dir") or ""
        local root_dir = target:extraconf("rules", "lorr.install_resources", "root_dir") or os.scriptdir()

        local abs_source = path.absolute(sourcefile)
        local rel_output = path.join(target:targetdir(), output_dir)
        if (root_dir ~= "" or root_dir ~= nil) then
            local rel_root = path.relative(path.directory(abs_source), root_dir)
            rel_output = path.join(rel_output, rel_root)
        end

        local abs_output = path.absolute(rel_output) .. "/" .. path.filename(sourcefile)
        batchcmds:show_progress(opt.progress, "${color.build.object}copying resource file %s", sourcefile)
        batchcmds:cp(abs_source, abs_output)

        batchcmds:add_depfiles(sourcefile)
        batchcmds:set_depmtime(os.mtime(abs_output))
        batchcmds:set_depcache(target:dependfile(abs_output))
    end)

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

    add_rpathdirs("@executable_path")
    add_includedirs("../", { public = true });
    add_files("**.cc")
    add_files("./Resources/**")
    add_rules("lorr.install_resources", {
        root_dir = os.scriptdir() .. "/Resources",
        output_dir = "resources",
    })

    add_options("profile")
    add_options("wayland")

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

target_end()
