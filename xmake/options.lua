option("profile")
    set_default(false)
    set_description("Enable application wide profiling.")
    add_defines("TRACY_ENABLE=1", { public = true })
    if is_plat("windows") then
        add_defines("TRACY_IMPORTS=1", { public = true })
    end
option_end()

