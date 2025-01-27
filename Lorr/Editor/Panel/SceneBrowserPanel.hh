#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct SceneBrowserPanel : PanelI {
    SceneBrowserPanel(std::string name_, bool open_ = true);

    void render(this SceneBrowserPanel &);
    void do_render(vuk::Format, vuk::Extent3D) override { render(); }
};
}  // namespace lr
