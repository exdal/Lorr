rule("lorr.install_resources")
    set_extensions(".ttf", ".slang", ".lua", ".txt")
    before_buildcmd_file(function (target, batchcmds, sourcefile, opt)
        local output_dir = target:extraconf("rules", "lorr.install_resources", "output_dir") or ""
        local root_dir = target:extraconf("rules", "lorr.install_resources", "root_dir") or os.scriptdir()

        local abs_source = path.absolute(sourcefile)
        local rel_output = path.join(target:targetdir(), output_dir)
        if (root_dir ~= "" or root_dir ~= nil) then
            local rel_root = path.relative(path.directory(abs_source), root_dir)
            rel_output = path.join(rel_output, rel_root)
        end

        local abs_output = path.absolute(rel_output) .. "/" .. path.filename(sourcefile)
        batchcmds:show_progress(opt.progress, "${color.build.object}copying resource file %s", sourcefile)
        batchcmds:cp(abs_source, abs_output)

        batchcmds:add_depfiles(sourcefile)
        batchcmds:set_depmtime(os.mtime(abs_output))
        batchcmds:set_depcache(target:dependfile(abs_output))
    end)

rule("lorr.debug")
    on_config(function (target)
        if is_mode("debug") then
            if not target:get("symbols") then
                target:set("symbols", "debug")
            end
            if not target:get("optimize") then
                target:set("optimize", "smallest")
            end
            target:add("cuflags", "-G")
            target:add("cxflags", "-DLS_DEBUG=1")
            target:add("cuflags", "-DLS_DEBUG=1")
        end
    end)

rule("lorr.release")
    on_config(function (target)
        if is_mode("release") then
            if not target:get("symbols") and target:kind() ~= "shared" then
                target:set("symbols", "hidden")
            end
            if not target:get("optimize") then
                target:set("optimize", "fastest")
            end

            if not target:get("strip") then
                target:set("strip", "all")
            end

            target:add("cxflags", "-DNDEBUG")
            target:add("cuflags", "-DNDEBUG")
        end
    end)

rule("lorr.releasedbg")
    on_config(function (target)
        if is_mode("releasedbg") then
            if not target:get("symbols") then
                target:set("symbols", "debug")
            end
            if not target:get("optimize") then
                target:set("optimize", "fastest")
            end

            if not target:get("strip") then
                target:set("strip", "all")
            end

            target:add("cuflags", "-lineinfo")
            target:add("cxflags", "-DNDEBUG")
            target:add("cuflags", "-DNDEBUG")
        end
    end)
