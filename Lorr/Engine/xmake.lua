target("Lorr")
    set_kind("static")
    set_languages("cxx23")
    add_forceincludes("Engine/pch.hh", { public = true, force = true })
    set_pcheader("Engine/pch.hh", { public = true, force = true })
    add_cxxflags("clang::-march=native", "clang_cl::/arch:AVX2", "cl::/arch:AVX2")

    if is_mode("debug") then
        add_ldflags(
            "-Wl,--export-dynamic",
            { tools = { "clang", "gcc" } })
    elseif is_mode("asan") then
        set_policy("build.sanitizer.address", true)
        set_policy("build.sanitizer.undefined", true)
        set_policy("build.sanitizer.leak", true)
    elseif is_mode("tsan") then
        set_policy("build.sanitizer.thread", true)
        set_optimize("faster")
        add_cxflags("-g")
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
        remove_files("OS/Win32*")
    end

    add_files("./Resources/**")
    add_rules("lorr.install_resources", {
        root_dir = os.scriptdir() .. "/Resources",
        output_dir = "resources",
    })

    add_options("profile")

    add_deps(
        "ls",
        { public = true })

    set_policy("package.precompiled", false)
    add_packages(
        "fmt",
        "libsdl3",
        "vk-bootstrap",
        "imgui",
        "slang-lorr",
        "tracy",
        "xxhash",
        "glm",
        "plf_colony",
        "simdutf",
        "simdjson",
        "unordered_dense",
        "lz4",
        "zstd",
        "fastgltf",
        "stb",
        "flecs",
        "imguizmo-lorr",
        "vuk",
        "meshoptimizer",
        "ktx-software",
        { public = true })

target_end()
