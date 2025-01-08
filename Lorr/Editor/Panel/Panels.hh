#pragma once

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace lr {
struct PanelI {
    std::string name = {};
    bool open = true;

    PanelI(std::string name_, bool open_ = true)
        : name(std::move(name_)),
          open(open_) {};

    virtual ~PanelI() = default;
    virtual void do_update() = 0;
    virtual void do_project_refresh() {}
};

}  // namespace lr
