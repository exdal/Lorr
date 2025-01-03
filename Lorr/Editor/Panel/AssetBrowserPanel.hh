#pragma once

#include "Editor/Panel/Panels.hh"

#include "Engine/Asset/UUID.hh"
#include "Engine/OS/File.hh"

namespace lr {
struct AssetDirectory {
    fs::path path = {};
    FileDescriptor watch_descriptor = FileDescriptor::Invalid;

    std::vector<fs::path> subdirs = {};
    std::vector<UUID> asset_uuids = {};

    auto watch(this AssetDirectory &, FileWatcher &watcher) -> void;
    auto add_subdir(this AssetDirectory &, const fs::path &path) -> void;
    auto add_asset(this AssetDirectory &, const fs::path &path) -> bool;
};

struct AssetBrowserPanel : PanelI {
    FileWatcher file_watcher = {};
    fs::path current_dir = {};
    fs::path home_dir = {};
    ankerl::unordered_dense::map<fs::path, AssetDirectory> dirs = {};

    AssetBrowserPanel(std::string name_, bool open_ = true);

    auto add_directory(this AssetBrowserPanel &, const fs::path &path) -> AssetDirectory *;
    auto remove_directory(this AssetBrowserPanel &, const fs::path &path) -> void;
    auto find_directory(this AssetBrowserPanel &, const fs::path &path) -> AssetDirectory *;

    void draw_dir_contents(this AssetBrowserPanel &);
    void draw_project_tree(this AssetBrowserPanel &);
    void draw_file_tree(this AssetBrowserPanel &, const fs::path &path);

    auto poll_watch_events(this AssetBrowserPanel &) -> void;
    void update(this AssetBrowserPanel &);

    void do_update() override { update(); }
};
}  // namespace lr
