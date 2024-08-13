#pragma once

#include "Engine/Core/Application.hh"

#include "Layout.hh"

namespace lr {
struct EditorApp : Application {
    EditorLayout layout = {};

    bool prepare(this EditorApp &);
    bool update(this EditorApp &, f32 delta_time);

    bool do_prepare() override { return prepare(); }
    bool do_update(f32 delta_time) override { return update(delta_time); }
};
}  // namespace lr
