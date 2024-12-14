toolchain("nix-clang")
    set_kind("standalone")
    set_toolset("cc", "clang")
    set_toolset("cxx", "clang++")
    set_toolset("ld", "clang++")
    set_toolset("sh", "clang++")
    set_toolset("ar", "llvm-ar")
    set_toolset("strip", "llvm-strip")
    set_toolset("ranlib", "llvm-ranlib")
    set_toolset("objcopy", "llvm-objcopy")
    set_toolset("mm", "clang")
    set_toolset("mxx", "clang++")
    set_toolset("as", "clang")
    set_toolset("mrc", "llvm-rc")

    on_load(function (toolchain)
        local march = is_arch("x86_64", "x64") and "-m64" or "-m32"
        toolchain:add("cxflags", march)
        toolchain:add("mxflags", march)
        toolchain:add("asflags", march)
        toolchain:add("ldflags", march)
        toolchain:add("shflags", march)

        -- Force LLVM stuff
        toolchain:add("cxflags", "-stdlib=libc++")
        toolchain:add("ldflags", "-fuse-ld=lld")

        -- NixOS env
        toolchain:add("includedirs", os.getenv("NIX_CLANG_INC"))
        toolchain:add("includedirs", os.getenv("NIX_LIBCXX_INC"))
        toolchain:add("includedirs", os.getenv("NIX_GLIBC_INC"))

        toolchain:add("linkdirs", os.getenv("NIX_GLIBC_LIB"))
    end)
