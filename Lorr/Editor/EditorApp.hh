#pragma once

#include "Engine/Core/Application.hh"

#include "EditorLayout.hh"

namespace lr {
struct EditorApp : Application {
    static EditorApp &get() { return dynamic_cast<EditorApp &>(Application::get()); }

    ankerl::unordered_dense::map<flecs::id_t, Icon::detail::icon_t> component_icons = {};
    EditorLayout layout = {};

    bool prepare(this EditorApp &);
    bool update(this EditorApp &, f32 delta_time);

    void do_shutdown() override {}
    bool do_prepare() override { return prepare(); }
    bool do_update(f32 delta_time) override { return update(delta_time); }
};
}  // namespace lr
