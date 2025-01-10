#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct InspectorPanel : PanelI {
    InspectorPanel(std::string name_, bool open_ = true);

    auto render(this InspectorPanel &) -> void;
    auto draw_inspector(this InspectorPanel &) -> void;

    void do_update() override {}
    void do_render(vuk::Format, vuk::Extent3D) override { render(); }
};
}  // namespace lr
