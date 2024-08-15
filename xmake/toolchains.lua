toolchain("lr-clang")
    set_kind("standalone")
    set_runtimes("c++_static", "c++_shared")

    set_toolset("cc",     "clang")
    set_toolset("cxx",    "clang++")
    set_toolset("mm",     "clang")
    set_toolset("cpp",    "clang -E")
    set_toolset("as",     "clang")
    set_toolset("ld",     "ld.lld")
    set_toolset("sh",     "ld.lld")
    set_toolset("ar",     "llvm-ar")
    set_toolset("strip",  "llvm-strip")
    set_toolset("ranlib", "llvm-ranlib")
    set_toolset("objcopy","llvm-objcopy")
    set_toolset("mrc",    "llvm-rc")


    on_load(function (toolchain)
        if toolchain:is_plat("windows") then
            toolchain:add("runtimes", "MT", "MTd", "MD", "MDd")
        end

        -- add march flags
        local march
        if toolchain:is_plat("windows") and not is_host("windows") then
            -- cross-compilation for windows
            if toolchain:is_arch("i386", "x86") then
                march = "-target i386-pc-windows-msvc"
            else
                march = "-target x86_64-pc-windows-msvc"
            end
            toolchain:add("ldflags", "-fuse-ld=lld")
            toolchain:add("shflags", "-fuse-ld=lld")
        elseif toolchain:is_arch("x86_64", "x64") then
            march = "-m64"
        elseif toolchain:is_arch("i386", "x86") then
            march = "-m32"
        end
        if march then
            toolchain:add("cxflags", march)
            toolchain:add("mxflags", march)
            toolchain:add("asflags", march)
        end
    end)
toolchain_end()
