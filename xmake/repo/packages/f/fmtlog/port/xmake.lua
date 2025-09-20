add_rules("mode.release", "mode.debug")
set_project("fmtlog")

add_requires("fmt 11.1.3", { system = false, configs = { header_only = false } })

target("fmtlog")
    set_kind("static")
    add_languages("cxx20")
    add_includedirs(".", { public = true })
    add_files("fmtlog.cc")
    add_packages("fmt")
    if is_plat("linux") then
        add_syslinks("pthread")
    end

target_end()
