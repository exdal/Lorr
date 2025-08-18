#pragma once

#include "Editor/Window/IWindow.hh"
#include "Engine/Scene/EditorCamera.hh"

#include <flecs.h>

namespace led {
struct ViewportWindow : IWindow {
    u32 gizmo_op = 0;
    lr::EditorCamera editor_camera = {};

    ViewportWindow(std::string name_, bool open_ = true);

    auto render(this ViewportWindow &, vuk::Format format, vuk::Extent3D extent) -> void;
    void do_render(vuk::Format format, vuk::Extent3D extent) override {
        render(format, extent);
    }
};
} // namespace lr
