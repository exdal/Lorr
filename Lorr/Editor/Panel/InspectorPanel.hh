#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct InspectorPanel : PanelI {
    InspectorPanel(std::string name_, bool open_ = true);

    auto update(this InspectorPanel &) -> void;
    auto draw_inspector(this InspectorPanel &) -> void;

    void do_update() override { update(); }
};
}  // namespace lr
