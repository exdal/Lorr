#pragma once

#include "Engine/Core/Application.hh"

#include "EditorLayout.hh"

namespace lr {
struct Project {
    fs::path root_dir = {};
};

struct EditorApp : Application {
    static EditorApp &get() { return dynamic_cast<EditorApp &>(Application::get()); }

    EditorLayout layout = {};
    ls::option<Project> active_project = ls::nullopt;
    flecs::entity selected_entity = {};
    ankerl::unordered_dense::set<fs::path> recent_projects = {};

    auto load_editor_data(this EditorApp &) -> void;
    auto save_editor_data(this EditorApp &) -> void;
    auto new_project(this EditorApp &, const fs::path &root_dir, const std::string &name) -> bool;
    auto open_project(this EditorApp &, const fs::path &path) -> bool;
    auto save_project(this EditorApp &) -> void;

    auto prepare(this EditorApp &) -> bool;
    auto update(this EditorApp &, f64 delta_time) -> bool;
    auto render(this EditorApp &, vuk::Format format, vuk::Extent3D extent) -> bool;
    auto shutdown(this EditorApp &) -> void;

    auto do_super_init([[maybe_unused]] ls::span<c8 *> args) -> bool override { return true; };
    auto do_shutdown() -> void override { shutdown(); }
    auto do_prepare() -> bool override { return prepare(); }
    auto do_update(f64 delta_time) -> bool override { return update(delta_time); }
    auto do_render(vuk::Format format, vuk::Extent3D extent) -> bool override { return render(format, extent); };
};
}  // namespace lr
