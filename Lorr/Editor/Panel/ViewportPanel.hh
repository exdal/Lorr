#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct ViewportPanel : PanelI {
    u32 gizmo_op = 0;

    ViewportPanel(std::string name_, bool open_ = true);

    auto on_drop(this ViewportPanel &) -> void;
    auto update(this ViewportPanel &) -> void;
    auto draw_viewport(this ViewportPanel &) -> void;

    void do_update() override { update(); }
    void do_project_refresh() override {}
};
}  // namespace lr
