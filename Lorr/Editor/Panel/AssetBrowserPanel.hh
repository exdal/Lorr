#pragma once

#include "Editor/Panel/Panels.hh"

#include "Engine/Asset/UUID.hh"
#include "Engine/OS/File.hh"

namespace lr {
struct AssetDirectory {
    fs::path path = {};
    FileWatcher *file_watcher = nullptr;
    FileDescriptor watch_descriptor = FileDescriptor::Invalid;

    AssetDirectory *parent = nullptr;
    std::deque<std::unique_ptr<AssetDirectory>> subdirs = {};
    ankerl::unordered_dense::set<UUID> asset_uuids = {};

    AssetDirectory(fs::path path_, FileWatcher *file_watcher_, AssetDirectory *parent_);
    ~AssetDirectory();
    auto add_subdir(this AssetDirectory &, const fs::path &path) -> AssetDirectory *;
    auto add_subdir(this AssetDirectory &, std::unique_ptr<AssetDirectory> &&directory) -> AssetDirectory *;
    auto add_asset(this AssetDirectory &, const fs::path &path) -> UUID;

    auto refresh(this AssetDirectory &) -> void;
};

struct AssetBrowserPanel : PanelI {
    FileWatcher file_watcher = {};
    AssetDirectory *current_dir = nullptr;
    std::unique_ptr<AssetDirectory> home_dir = nullptr;

    AssetBrowserPanel(std::string name_, bool open_ = true);

    auto add_directory(this AssetBrowserPanel &, const fs::path &path) -> std::unique_ptr<AssetDirectory>;
    auto remove_directory(this AssetBrowserPanel &, const fs::path &path) -> void;
    auto remove_directory(this AssetBrowserPanel &, AssetDirectory *directory) -> void;
    auto find_directory(this AssetBrowserPanel &, const fs::path &path) -> AssetDirectory *;

    void draw_dir_contents(this AssetBrowserPanel &);
    void draw_project_tree(this AssetBrowserPanel &);
    void draw_file_tree(this AssetBrowserPanel &, AssetDirectory *directory);

    auto poll_watch_events(this AssetBrowserPanel &) -> void;
    void render(this AssetBrowserPanel &);
    void do_render(vuk::Format, vuk::Extent3D) override {
        render();
    }
};
} // namespace lr
