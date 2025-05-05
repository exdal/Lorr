#pragma once

#include "Editor/Window/IWindow.hh"

namespace led {
struct WelcomeWindow : IWindow {
    WelcomeWindow(std::string name_, bool open_ = true);

    auto render(this WelcomeWindow &, vuk::Format format, vuk::Extent3D extent) -> void;
    void do_render(vuk::Format format, vuk::Extent3D extent) override {
        render(format, extent);
    }
};
} // namespace lr

