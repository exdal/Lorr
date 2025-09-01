add_rules("mode.release", "mode.debug")
set_project("vuk")

add_requires("fmt 11.1.4", { system = false, configs = { header_only = false } })
add_requires("vulkan-memory-allocator v3.1.0")
add_requires("concurrentqueue v1.0.4")
add_requires("plf_colony v7.41")
add_requires("robin-hood-hashing 3.11.5")
add_requires("stb 2024.06.01")
add_requires("function2 4.2.4")
add_requires("spirv-cross 1.4.309+0")
add_requires("small_vector 2024.12.23")
add_requires("vk-bootstrap v1.4.307")

option("debug_allocations")
    set_default(false)
    set_showmenu(true)
    add_defines("VUK_DEBUG_ALLOCATIONS=1", { force = true, public = true })

option("disable_exceptions")
    set_default(false)
    set_showmenu(true)
    add_defines("VUK_DISABLE_EXCEPTIONS=1", { force = true, public = true })

target("vuk")
    set_kind("static")
    add_languages("cxx20")
    add_includedirs("include/", { public = true })

    add_files("src/*.cpp")
    add_files("src/runtime/**.cpp")

    set_options("debug_allocations", "disable_exceptions")

    -- public packages
    add_packages(
        "fmt",
        "robin-hood-hashing",
        "plf_colony",
        "function2",
        "small_vector",
        "vulkan-memory-allocator",
        "vk-bootstrap",
        { public = true })

    -- private packages
    add_packages(
        "concurrentqueue",
        "stb",
        "spirv-cross",
        { public = false })

    if is_os("windows") then
        add_defines(
            "NOMINMAX",
            "VC_EXTRALEAN",
            "WIN32_LEAN_AND_MEAN",
            "_CRT_SECURE_NO_WARNINGS",
            "_SCL_SECURE_NO_WARNINGS",
            "_SILENCE_CLANG_CONCEPTS_MESSAGE",
            "_SILENCE_CXX23_ALIGNED_STORAGE_DEPRECATION_WARNING",
            { public = true })
    end

    on_config(function (target)
        if target:has_tool("cxx", "msvc", "cl") then
            target:add("defines", "VUK_COMPILER_MSVC=1", { force = true, public = true })
            target:add("cxflags", "/permissive- /Zc:char8_t- /wd4068", { public = false })
        elseif target:has_tool("cxx", "clang_cl", "clang-cl") then
            target:add("defines", "VUK_COMPILER_CLANGCL=1", { force = true, public = true })
            target:add("cxflags", "-Wno-nullability-completeness", { public = false })
            target:add("cxflags", "/permissive- /Zc:char8_t- /wd4068", { public = false })
        elseif target:has_tool("cxx", "clang", "clangxx") then
            target:add("defines", "VUK_COMPILER_CLANGPP=1", { force = true, public = true })
            target:add("cxflags", "-fno-char8_t -Wno-nullability-completeness -fms-extensions", { public = false })
        elseif target:has_tool("cxx", "gcc", "gxx") then
            target:add("defines", "VUK_COMPILER_GPP=1", { force = true, public = true })
            target:add("cxflags", "-fno-char8_t", { public = false })
        end
    end)
target_end()
