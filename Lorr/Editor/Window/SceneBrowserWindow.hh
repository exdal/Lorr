#pragma once

#include "Editor/Window/IWindow.hh"

namespace led {
struct SceneBrowserWindow : IWindow {
    SceneBrowserWindow(std::string name_, bool open_ = true);

    void render(this SceneBrowserWindow &);
    void do_render(vuk::Swapchain &) override {
        render();
    }
};
} // namespace led
