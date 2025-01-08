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

    void update(this ConsolePanel &);

    void do_update() override { update(); }
};
}  // namespace lr
