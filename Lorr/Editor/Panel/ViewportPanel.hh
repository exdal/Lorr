#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct ViewportPanel : PanelI {
    u32 gizmo_op = 0;

    ViewportPanel(std::string name_, bool open_ = true);

    auto on_drop(this ViewportPanel &) -> void;
    auto render(this ViewportPanel &, vuk::Format format, vuk::Extent3D extent) -> void;
    auto draw_viewport(this ViewportPanel &, vuk::Format format, vuk::Extent3D extent) -> void;
    void do_render(vuk::Format format, vuk::Extent3D extent) override { render(format, extent); }
};
}  // namespace lr
