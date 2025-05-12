target("Editor")
    set_kind("binary")
    set_languages("cxx23")
    add_deps("Lorr")
    add_includedirs("./")
    add_files("**.cc")
    add_rpathdirs("@executable_path")

    add_files("./Resources/**")
    add_rules("lorr.install_resources", {
        root_dir = os.scriptdir() .. "/Resources",
        output_dir = "resources/editor",
    })
target_end()

