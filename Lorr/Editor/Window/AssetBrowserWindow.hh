#pragma once

#include "Editor/Window/IWindow.hh"

#include "Engine/Asset/UUID.hh"

namespace led {
struct AssetDirectory {
    fs::path path = {};

    AssetDirectory *parent = nullptr;
    std::deque<std::unique_ptr<AssetDirectory>> subdirs = {};
    ankerl::unordered_dense::set<lr::UUID> asset_uuids = {};

    AssetDirectory(fs::path path_, AssetDirectory *parent_);
    ~AssetDirectory();
    auto add_subdir(this AssetDirectory &, const fs::path &path) -> AssetDirectory *;
    auto add_subdir(this AssetDirectory &, std::unique_ptr<AssetDirectory> &&directory) -> AssetDirectory *;
    auto add_asset(this AssetDirectory &, const fs::path &path) -> lr::UUID;

    auto refresh(this AssetDirectory &) -> void;
};

struct AssetBrowserWindow : IWindow {
    AssetDirectory *current_dir = nullptr;
    std::unique_ptr<AssetDirectory> home_dir = nullptr;

    AssetBrowserWindow(std::string name_, bool open_ = true);

    auto add_directory(this AssetBrowserWindow &, const fs::path &path) -> std::unique_ptr<AssetDirectory>;
    auto remove_directory(this AssetBrowserWindow &, const fs::path &path) -> void;
    auto remove_directory(this AssetBrowserWindow &, AssetDirectory *directory) -> void;
    auto find_directory(this AssetBrowserWindow &, const fs::path &path) -> AssetDirectory *;

    void render(this AssetBrowserWindow &);
    void do_render(vuk::Swapchain &) override {
        render();
    }
};
} // namespace led
