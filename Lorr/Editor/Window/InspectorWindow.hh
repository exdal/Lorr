#pragma once

#include "Editor/Window/IWindow.hh"

namespace led {
struct InspectorWindow : IWindow {
    InspectorWindow(std::string name_, bool open_ = true);

    auto render(this InspectorWindow &) -> void;
    void do_render(vuk::Format, vuk::Extent3D) override {
        render();
    }
};
} // namespace lr
