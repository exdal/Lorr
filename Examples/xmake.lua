local add_example = function(example_name, srcs)
    target(example_name)
      set_kind("binary")
      set_languages("cxx20")
      add_deps("Lorr")
      add_files(srcs)
      add_includedirs("Base")
    target_end()
end

add_example("Triangle", "Triangle/main.cc")
add_example("Task", "Task/main.cc")
