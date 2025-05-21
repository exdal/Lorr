target("Lorr")
    set_kind("static")
    set_languages("cxx23")
    add_forceincludes("Engine/pch.hh", { public = true, force = true })
    set_pcheader("Engine/pch.hh", { public = true, force = true })
    add_cxxflags("clang::-march=native", "clang_cl::/arch:AVX2", "cl::/arch:AVX2")

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

    add_packages(
        "fmt",
        "fmtlog-lr",
        "libsdl3",
        "vk-bootstrap",
        "imgui",
        "imguizmo-lr",
        "shader-slang",
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
        "vuk",
        "meshoptimizer",
        "ktx-software",
        { public = true })

target_end()
