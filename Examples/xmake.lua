local add_example = function(example_name, srcs, shaders)
    target(example_name)
      set_kind("binary")
      set_languages("cxx20")
      add_deps("Lorr")
      add_files(srcs)
      add_includedirs("Base")
      on_load(function (target)
        import("lib.detect.find_file")
        table.insert(shaders, { "IMGUI_SHADER_PATH", "Base/imgui.slang" })
        for _, v in pairs(shaders) do
          target:add("defines", v[1] .. "=\"$(projectdir)/Examples/" .. v[2] .. "\"", { public = true })
        end
      end)
    target_end()
end

add_example("Triangle", "Triangle/main.cc", { { "TRIANGLE_SHADER_PATH", "Triangle/triangle.slang" } })
add_example("Compute", "Compute/main.cc", { { "COMPUTE_SHADER_PATH", "Compute/compute.slang" } })
add_example("Task", "Task/main.cc", {})
