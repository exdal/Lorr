package("shader-slang")
    set_homepage("https://github.com/shader-slang/slang")
    set_description("Making it easier to work with shaders")
    set_license("MIT")

    if is_host("windows") then
        add_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-windows-x86_64.tar.gz",
            {version = function (version) return version:gsub("v", "") end})

        add_versions("v2025.10.4", "f4199d9cb32f93410444713adfe880da2b665a9e13f2f8e23fdbff06068a9ff3")
        add_versions("v2025.12.1", "02018cc923a46c434e23b166ef13c14165b0a0c4b863279731c4f6c4898fbf8e")
        add_versions("v2025.15",   "f37e7215e51bee4e8f5ec7b84a5d783deb6cbd0bd033c026b94f2d5a31e88d28")
        add_versions("v2025.16",   "5d6f01208e502d8365d905ba0f4102c9af476d36f33d834107e89ecf0463bc61")
        add_versions("v2025.16.1", "26a5acb8f03f0a664d04842df15567de9db6d46db17621efb94469a70d6dce70")
        add_versions("v2025.17.2", "08faa31d1efa6ce69f9ec3bccd6ea5419cb2729a11df4e9e109a3932ab1d0940")
    elseif is_host("linux") then
        add_urls("https://github.com/shader-slang/slang/releases/download/v$(version)/slang-$(version)-linux-x86_64.tar.gz",
            {version = function (version) return version:gsub("v", "") end})

        add_versions("v2025.10.4", "c2edcfdada38feb345725613c516a842700437f6fa55910b567b9058c415ce8f")
        add_versions("v2025.12.1", "8f34b98391562ce6f97d899e934645e2c4466a02e66b69f69651ff1468553b27")
        add_versions("v2025.15",   "1eaa24f1f0483f8b8cc4b95153c815394d2f6cae08dbaf8b18d6b7975b8bbe03")
        add_versions("v2025.16",   "2db64f788eadd2742280752334439c7f540581dfa59d23c1a56e06556e5b8405")
        add_versions("v2025.16.1", "059d5e5ccafd1107ac5965b95706426c68afccfe7f720f1359ee877b41b31a2a")
        add_versions("v2025.17.2", "663cf5ea54da685b5b31e8fd2d2904e55936174b2901055e33fffbe249dd9ce3")
    end

    on_install("windows|x64", "linux|x86_64", function (package)
        os.cp("include/*.h", package:installdir("include"))

        if package:is_plat("windows") then
            os.trycp("lib/slang.*", package:installdir("lib"))
            os.trycp("bin/slang.*", package:installdir("lib"))
        else
            os.trycp("lib/libslang.*", package:installdir("lib"))
            os.trycp("bin/libslang.*", package:installdir("lib"))
        end

        os.trycp("lib/*slang-glslang.*", package:installdir("lib"))
        os.trycp("bin/*slang-glslang.*", package:installdir("lib"))

        os.trycp("lib/*slang-glsl-module.*", package:installdir("lib"))
        os.trycp("bin/*slang-glsl-module.*", package:installdir("lib"))

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

package_end()
