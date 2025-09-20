package("small_vector")
    set_kind("library", {headeronly = true})
    set_homepage("https://github.com/gharveymn/small_vector")
    set_description("MIT")

    add_urls("https://github.com/gharveymn/small_vector.git")
    add_versions("2024.12.23", "5b4ad3bd3dc3e1593a7e95cb3843a87b5ae21000")

    add_includedirs("include", "include/stb")

    on_install(function (package)
        os.cp("source/include/gch", package:installdir("include"))
    end)
package_end()


