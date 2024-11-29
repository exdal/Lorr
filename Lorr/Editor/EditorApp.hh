#pragma once

#include "Engine/Core/Application.hh"

#include "EditorLayout.hh"

namespace lr {
struct EditorApp : Application {
    static EditorApp &get() { return dynamic_cast<EditorApp &>(Application::get()); }

    EditorLayout layout = {};

    bool prepare(this EditorApp &self);
    bool update(this EditorApp &self, f64 delta_time);

    bool do_super_init([[maybe_unused]] ls::span<c8 *> args) override { return true; };
    void do_shutdown() override {}
    bool do_prepare() override { return prepare(); }
    bool do_update(f64 delta_time) override { return update(delta_time); }
};
}  // namespace lr
