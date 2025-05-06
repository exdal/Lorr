#pragma once

#include "Editor/Window/IWindow.hh"
#include "Editor/Project.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Graphics/FrameProfiler.hh"

namespace led {
struct EditorApp : lr::Application {
    static EditorApp &get() {
        return dynamic_cast<EditorApp &>(Application::get());
    }

    std::unique_ptr<Project> active_project = nullptr;
    ankerl::unordered_dense::map<fs::path, ProjectFileInfo> recent_project_infos = {};

    std::vector<std::unique_ptr<IWindow>> windows = {};
    ls::option<u32> dockspace_id = ls::nullopt;

    ankerl::unordered_dense::map<std::string, lr::UUID> editor_assets = {};
    lr::FrameProfiler frame_profiler = {};

    bool show_profiler = false;

    auto load_editor_data(this EditorApp &) -> void;
    auto save_editor_data(this EditorApp &) -> void;

    auto new_project(this EditorApp &, const fs::path &root_dir, const std::string &name) -> std::unique_ptr<Project>;
    auto open_project(this EditorApp &, const fs::path &path) -> std::unique_ptr<Project>;
    auto save_project(this EditorApp &, std::unique_ptr<Project> &project) -> void;
    auto set_active_project(this EditorApp &, std::unique_ptr<Project> &&project) -> void;

    auto get_asset_texture(this EditorApp &, lr::Asset *asset) -> lr::Texture *;
    auto get_asset_texture(this EditorApp &, lr::AssetType asset_type) -> lr::Texture *;

    auto prepare(this EditorApp &) -> bool;
    auto update(this EditorApp &, f64 delta_time) -> bool;
    auto render(this EditorApp &, vuk::Format format, vuk::Extent3D extent) -> bool;
    auto shutdown(this EditorApp &) -> void;

    template<typename T>
    auto add_window(this EditorApp &self, std::string name, const c8 *icon = nullptr, bool open = true) -> std::pair<usize, T *> {
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

    auto remove_window(this EditorApp &, usize window_id) -> void;

    auto do_super_init([[maybe_unused]] ls::span<c8 *> args) -> bool override {
        return true;
    };
    auto do_shutdown() -> void override {
        shutdown();
    }
    auto do_prepare() -> bool override {
        return prepare();
    }
    auto do_update(f64 delta_time) -> bool override {
        return update(delta_time);
    }
    auto do_render(vuk::Format format, vuk::Extent3D extent) -> bool override {
        return render(format, extent);
    };
};
} // namespace led

// ImGui extension

namespace ImGui {
bool drag_vec(i32 id, void *data, usize components, ImGuiDataType data_type);
void center_text(std::string_view str);
bool image_button(std::string_view text, ImTextureID texture_id, const ImVec2 &button_size);
void text_sv(std::string_view str);
} // namespace ImGui
