#pragma once

#include "Editor/Project.hh"
#include "Editor/Window/IWindow.hh"

#include "Engine/Asset/Asset.hh"

#include "Engine/Graphics/FrameProfiler.hh"

namespace led {
struct EditorModule {
    constexpr static auto MODULE_NAME = "Editor";

    std::unique_ptr<Project> active_project = nullptr;
    ankerl::unordered_dense::map<fs::path, ProjectFileInfo> recent_project_infos = {};

    std::vector<std::unique_ptr<IWindow>> windows = {};
    ls::option<u32> dockspace_id = ls::nullopt;

    ankerl::unordered_dense::map<std::string, lr::UUID> editor_assets = {};
    lr::FrameProfiler frame_profiler = {};

    bool show_profiler = false;
    bool show_debug = false;

    auto load_editor_data(this EditorModule &) -> void;
    auto save_editor_data(this EditorModule &) -> void;

    auto new_project(this EditorModule &, const fs::path &root_path, const std::string &name) -> std::unique_ptr<Project>;
    auto open_project(this EditorModule &, const fs::path &path) -> std::unique_ptr<Project>;
    auto save_project(this EditorModule &, std::unique_ptr<Project> &project) -> void;
    auto set_active_project(this EditorModule &, std::unique_ptr<Project> &&project) -> void;

    auto get_asset_texture(this EditorModule &, lr::Asset *asset) -> lr::Texture *;
    auto get_asset_texture(this EditorModule &, lr::AssetType asset_type) -> lr::Texture *;

    auto init(this EditorModule &) -> bool;
    auto update(this EditorModule &, f64 delta_time) -> bool;
    auto render(this EditorModule &, vuk::Swapchain &swap_chain) -> bool;
    auto destroy(this EditorModule &) -> void;

    template<typename T>
    auto add_window(this EditorModule &self, std::string name, const c8 *icon = nullptr, bool open = true) -> std::pair<usize, T *> {
        auto window_index = self.windows.size();
        auto icon_name = std::string{};
        if (icon) {
            icon_name = fmt::format("{}  {}", icon, name);
        } else {
            icon_name = std::move(name);
        }

        auto window = std::make_unique<T>(std::move(icon_name), open);
        T *window_ptr = window.get();
        self.windows.push_back(std::move(window));
        return { window_index, window_ptr };
    }

    auto remove_window(this EditorModule &, usize window_id) -> void;
};
} // namespace led

// ImGui extension
namespace ImGui {
bool drag_vec(i32 id, void *data, usize components, ImGuiDataType data_type);
void center_text(std::string_view str);
bool image_button(std::string_view text, ImTextureID texture_id, const ImVec2 &button_size);
void text_sv(std::string_view str);
} // namespace ImGui
