#pragma once

#include "Editor/Window/IWindow.hh"

namespace led {
struct ConsoleWindow : IWindow {
    struct Message {
        u32 verbosity = {};
        std::string message = {};
    };

    std::vector<Message> messages = {};

    ConsoleWindow(std::string name_, bool open_ = true);

    void render(this ConsoleWindow &);
    void do_render(vuk::Format, vuk::Extent3D) override {
        render();
    }
};
} // namespace lr
