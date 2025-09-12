add_repositories("local repo", {rootdir = os.scriptdir()})

local fmt_version = "11.2.0"
local fmt_configs = { header_only = false, shared = false }
add_requires("fmt " .. fmt_version, { configs = fmt_configs, system = false })
add_requires("fmtlog v2.3.0", { configs = {
    shared = false,
}, system = false })
add_requireconfs("fmt", "fmtlog.fmt", {
    override = true,
    version = fmt_version,
    configs = fmt_configs,
    system = false
})

add_requires("xxhash v0.8.3", {system = false})
add_requires("glm 1.0.1", { configs = {
    header_only = true,
    cxx_standard = "20",
}, system = false })
add_requires("plf_colony v7.41", {system = false})

local imgui_version = "v1.92.0-docking"
local imgui_configs = { wchar32 = true }
add_requires("imgui " .. imgui_version, { configs = imgui_configs, system = false })

add_requires("implot 3da8bd34299965d3b0ab124df743fe3e076fa222")
add_requireconfs("imgui", "implot.imgui", {
    override = true,
    version = imgui_version,
    configs = imgui_configs,
    system = false,
})

add_requires("imguizmo 1.91.3+wip")
add_requireconfs("imgui", "imguizmo.imgui", {
    override = true, version = imgui_version, configs = imgui_configs
})

add_requires("simdutf v6.2.0", {system = false})
add_requires("simdjson v3.12.2", {system = false})
add_requires("unordered_dense v4.5.0", {system = false})
add_requires("tracy v0.11.1", { configs = {
    tracy_enable = false,
    on_demand = true,
    callstack = false,
    callstack_inlines = false,
    code_transfer = false,
    exit = true,
    system_tracing = false,
    system = false,
} })
add_requires("vk-bootstrap v1.4.307", { system = false, debug  = is_mode("debug") })
add_requires("fastgltf v0.8.0", {system = false})
add_requires("stb 2024.06.01", {system = false})
add_requires("lz4 v1.10.0", {system = false})
add_requires("zstd v1.5.6", {system = false})
add_requires("flecs v4.0.4", {system = false})

add_requires("libsdl3 3.2.16", { configs = {
    wayland = false
}, system = false})

add_requires("shader-slang v2025.15", {system = false})
add_requires("vuk 2025.09.01", { configs = {
    debug_allocations = false,
    disable_exceptions = false,
}, debug = is_mode("debug"), system = false })

add_requires("meshoptimizer v0.24", {system = false})
add_requires("ktx v4.4.0", { debug = is_plat("windows"), system = false })

add_requires("svector v1.0.3", {system = false})
