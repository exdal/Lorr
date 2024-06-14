local add_example = function(example_name, srcs)
    target(example_name)
      set_kind("binary")
      set_languages("cxx20")
      add_deps("Lorr")
      add_files(srcs)
    target_end()
end

if has_config("build_examples") then
    add_example("Triangle", "Triangle/main.cc")
    add_example("Task", "Task/main.cc")
    add_example("Testbed", "Testbed/main.cc")
end
