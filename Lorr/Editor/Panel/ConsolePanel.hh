#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct ConsolePanel : PanelI {
    struct Message {
        u32 verbosity = {};
        std::string message = {};
    };

    std::vector<Message> messages = {};

    ConsolePanel(std::string name_, bool open_ = true);

    void render(this ConsolePanel &);
    void do_render(vuk::Format, vuk::Extent3D) override {
        render();
    }
};
} // namespace lr
