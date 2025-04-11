local add_generator = function(dir, options)
    options = options or {}
    target(options.name or dir)
        set_default(false)
        set_kind(options.kind or "binary")
        set_languages("cxx17")
        set_warnings("none")
        add_rpathdirs("@executable_path")

        set_policy("build.fence", true)

        set_group("generators")
        set_targetdir("$(projectdir)/generators")
        set_installdir("$(projectdir)/generators")

        add_files(dir .. "/*.cpp")
        if options.includes then
            add_includedirs(options.includes, { public = true })
        end

        add_deps("core")

        if options.deps then
            for _, dep in ipairs(options.deps) do
                add_deps(dep)
            end
        end

        if options.links then
            for _, dep in ipairs(options.links) do
                add_links(dep)
            end
        end

        if options.defines then
            for _, v in ipairs(options.defs) do
                add_defines(v)
            end
        end

    target_end()
end

add_generator("slang-embed")
add_generator("slang-generate")
add_generator("slang-lookup-generator", { deps = { "compiler-core" } })
add_generator("slang-capability-generator", { deps = { "compiler-core" } })
add_generator("slang-spirv-embed-generator", { deps = { "compiler-core" } })

add_generator("slang-cpp-parser", { kind = "static", includes = ".", deps = { "compiler-core" } })
add_generator("slang-cpp-extractor", { deps = { "compiler-core", "slang-cpp-parser" } })

add_generator("$(projectdir)/source/slangc", {
    name = "slang-bootstrap",
    deps = {
        "prelude",
        "slang-capability-lookup",
        "slang-lookup-tables",
        "slang-without-embedded-core-module", -- needs to stay here for fence
    },
    links = {
        "slang-without-embedded-core-module",
    },
    defines = {
        "SLANG_BOOTSTRAP=1",
    },
})

