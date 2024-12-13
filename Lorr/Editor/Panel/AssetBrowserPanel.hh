#pragma once

#include "Editor/Panel/Panels.hh"

#include "Engine/Asset/Asset.hh"
#include "Engine/OS/File.hh"

namespace lr {
struct AssetDirectory {
    fs::path path = {};
    FileDescriptor watch_descriptor = FileDescriptor::Invalid;

    AssetDirectory *parent = nullptr;
    std::vector<std::unique_ptr<AssetDirectory>> subdirs = {};
    std::vector<Asset *> assets = {};

    static auto create(const fs::path &path_) -> std::unique_ptr<AssetDirectory>;
    auto watch(FileWatcher &watcher) -> void;
    auto add_subdir(std::unique_ptr<AssetDirectory> &&dir_) -> void;
    auto add_asset(const fs::path &path_) -> bool;
};

struct AssetBrowserPanel : PanelI {
    FileWatcher file_watcher = {};
    AssetDirectory *current_dir = nullptr;
    std::unique_ptr<AssetDirectory> home_dir = nullptr;

    AssetBrowserPanel(std::string name_, bool open_ = true);

    auto add_directory(this AssetBrowserPanel &, const fs::path &root_dir) -> std::unique_ptr<AssetDirectory>;
    auto find_directory_by_path(this AssetBrowserPanel &, const fs::path &path) -> AssetDirectory *;
    auto find_closest_directory_by_path(this AssetBrowserPanel &, const fs::path &path) -> AssetDirectory *;

    void draw_dir_contents(this AssetBrowserPanel &);
    void draw_project_tree(this AssetBrowserPanel &);
    void draw_file_tree(this AssetBrowserPanel &, AssetDirectory *directory);

    auto poll_watch_events(this AssetBrowserPanel &) -> void;
    void update(this AssetBrowserPanel &);

    void do_update() override { update(); }
};
}  // namespace lr
