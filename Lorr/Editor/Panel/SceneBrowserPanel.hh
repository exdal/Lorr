#pragma once

#include "Editor/Panel/Panels.hh"

namespace lr {
struct SceneBrowserPanel : PanelI {
    SceneBrowserPanel(std::string name_, bool open_ = true);

    void update(this SceneBrowserPanel &);

    void do_update() override { update(); }
};
}  // namespace lr
