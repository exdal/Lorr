#pragma once

#include "Editor/Window/IWindow.hh"
#include "Engine/Scene/EditorCamera.hh"

#include <flecs.h>

namespace led {
struct ViewportWindow : IWindow {
    u32 gizmo_op = 0;
    lr::EditorCamera editor_camera = {};

    ViewportWindow(std::string name_, bool open_ = true);

    auto render(this ViewportWindow &, vuk::Swapchain &swap_chain) -> void;
    void do_render(vuk::Swapchain &swap_chain) override {
        render(swap_chain);
    }
};
} // namespace led
