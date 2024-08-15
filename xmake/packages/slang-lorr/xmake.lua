package("slang-lorr")
    set_homepage("https://github.com/shader-slang/slang")
    set_description("Making it easier to work with shaders")
    set_license("MIT")

    add_urls("https://github.com/shader-slang/slang.git")

    add_versions("v2024.9.1",  "9b580e58417a77109617804362be872f05885f23")
    add_versions("v2024.1.22", "c00f461aad3d997a2e1c59559421275d6339ae6f")
    add_versions("v2024.1.18", "efdbb954c57b89362e390f955d45f90e59d66878")
    add_versions("v2024.1.17", "62b7219e715bd4c0f984bcd98c9767fb6422c78f")

    add_patches("v2024.9.1",  path.join(os.scriptdir(), "patches", "2024.9.1",  "clang.patch"), "2b3267cc8235ee0668444577c75e03cdb1325c619922bf925bf17867a4d366a1")
    add_patches("v2024.1.22", path.join(os.scriptdir(), "patches", "2024.1.22", "clang.patch"), "0e651f450e94b5517099d3489f69acb75970a4256fe1bb763205ef544530ef4d")
    add_patches("v2024.1.22", path.join(os.scriptdir(), "patches", "2024.1.22", "warnings.patch"), "0f77493869660b8d34ccaeeec4ca358804423303b72e5cb430b73d0a888d3fff")

    add_configs("shared", { description = "Build shared library", default = true, type = "boolean", readonly = true })
    add_configs("embed_stdlib_source", { description = "Embed stdlib source in the binary", default = true, type = "boolean" })
    add_configs("embed_stdlib", { description = "Build slang with an embedded version of the stdlib", default = true, type = "boolean" })
    add_configs("full_ir_validation", { description = "Enable full IR validation (SLOW!)", default = false, type = "boolean" })
    add_configs("gfx", { description = "Enable gfx targets", default = false, type = "boolean" })
    add_configs("slangd", { description = "Enable language server target", default = false, type = "boolean" })
    add_configs("slangc", { description = "Enable standalone compiler target", default = false, type = "boolean" })
    add_configs("slangrt", { description = "Enable runtime target", default = false, type = "boolean" })
    add_configs("slang_glslang", { description = "Enable glslang dependency and slang-glslang wrapper target", default = true, type = "boolean" })
    add_configs("slang_llvm_flavor", { description = "How to get or build slang-llvm (available options: FETCH_BINARY, USE_SYSTEM_LLVM, DISABLE)", default = "DISABLE", type = "string" })

    add_deps("cmake")

    on_install("windows|x64", "macosx", "linux|x86_64", function (package)
        local configs = {"-DSLANG_ENABLE_TESTS=OFF", "-DSLANG_ENABLE_EXAMPLES=OFF"}
        table.insert(configs, "-DCMAKE_BUILD_TYPE=" .. (package:is_debug() and "Debug" or "Release"))
        table.insert(configs, "-DSLANG_LIB_TYPE=" .. (package:config("shared") and "SHARED" or "STATIC"))
        table.insert(configs, "-DSLANG_EMBED_STDLIB_SOURCE=" .. (package:config("embed_stdlib_source") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_EMBED_STDLIB=" .. (package:config("embed_stdlib") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_FULL_IR_VALIDATION=" .. (package:config("full_ir_validation") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_ASAN=" .. (package:config("asan") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_GFX=" .. (package:config("gfx") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_SLANGD=" .. (package:config("slangd") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_SLANGC=" .. (package:config("slangc") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_SLANGRT=" .. (package:config("slangrt") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_ENABLE_SLANG_GLSLANG=" .. (package:config("slang_glslang") and "ON" or "OFF"))
        table.insert(configs, "-DSLANG_SLANG_LLVM_FLAVOR=" .. package:config("slang_llvm_flavor"))

        import("package.tools.cmake").install(package, configs)
        package:addenv("PATH", "bin")
    end)

    on_test(function (package)
        assert(package:check_cxxsnippets({ test = [[
            #include <slang-com-ptr.h>
            #include <slang.h>

            void test() {
                Slang::ComPtr<slang::IGlobalSession> global_session;
                slang::createGlobalSession(global_session.writeRef());
            }
        ]] }, {configs = {languages = "c++17"}}))
    end)

