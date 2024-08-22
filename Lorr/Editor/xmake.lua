target("Editor")
    set_kind("binary")
    set_languages("cxx23")
    add_deps("Lorr")
    add_includedirs("./")
    add_files("**.cc")
target_end()

