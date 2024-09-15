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
    add_includedirs("../", { public = true })
    add_files("**.cc")
    if is_plat("windows") then
        add_syslinks("gdi32", "msimg32", "user32")
        remove_files("OS/Linux*")
    elseif is_plat("linux") then
        if not has_config("wayland") then
            add_syslinks("xcb")
        end
        remove_files("OS/Win32*")
    end

    add_files("./Resources/**")
    add_rules("lorr.install_resources", {
        root_dir = os.scriptdir() .. "/Resources",
        output_dir = "resources",
    })

    add_options("profile")
    add_options("wayland")

    add_deps(
        "ls",
        { public = true })

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
        "flecs",
        "glaze",
        "imguizmo-lorr",
        { public = true })

target_end()
