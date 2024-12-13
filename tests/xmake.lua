add_rules("mode.debug", "mode.release")

if has_config("enable_tests") then
    for _, file in ipairs(os.files("test_*.cc")) do
        local name = path.basename(file)
        target(name)
            set_kind("binary")
            set_default(false)
            set_languages("cxx23")
            add_files(name .. ".cc")
            add_tests("default", { runargs = { "--gmock_verbose=info", "--gtest_stack_trace_depth=10" } })
            add_packages("gtest")
            add_deps("Lorr")
            if is_plat("windows") then
                add_ldflags("/subsystem:console")
            end
    end
end
