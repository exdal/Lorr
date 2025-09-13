#pragma once

#include "Editor/Window/IWindow.hh"

namespace led {
struct ConsoleWindow : IWindow {
    ConsoleWindow(std::string name_, bool open_ = true);

    void render(this ConsoleWindow &);
    void do_render(vuk::Swapchain &) override {
        render();
    }
};
} // namespace led
