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

