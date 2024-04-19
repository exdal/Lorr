return {
  shaders = {
    include_dirs = { "shaders" },
    files = {
      { "triangle.slang", vs_entry = "main_vs", ps_entry = "main_ps" }
    }
  }
}
