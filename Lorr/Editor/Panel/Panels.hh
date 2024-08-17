#pragma once

#include <imgui.h>
#include <imgui_internal.h>

namespace lr {
struct PanelI {
    std::string_view name = {};
    c8 icon_code = 0;
    bool open = true;

    PanelI(std::string_view name_, c8 icon_code_, bool open_ = true)
        : name(name_),
          icon_code(icon_code_),
          open(open_) {};

    virtual ~PanelI() = default;
    virtual void do_update() = 0;
};

struct ToolsPanel : PanelI {
    ToolsPanel(std::string_view name_, c8 icon_code_, bool open_ = true);

    void update(this ToolsPanel &);

    void do_update() override { update(); }
};

struct SceneBrowserPanel : PanelI {
    SceneBrowserPanel(std::string_view name_, c8 icon_code_, bool open_ = true);

    void update(this SceneBrowserPanel &);

    void do_update() override { update(); }
};

struct ViewportPanel : PanelI {
    ViewportPanel(std::string_view name_, c8 icon_code_, bool open_ = true);

    void update(this ViewportPanel &);

    void do_update() override { update(); }
};

struct InspectorPanel : PanelI {
    InspectorPanel(std::string_view name_, c8 icon_code_, bool open_ = true);

    void update(this InspectorPanel &);

    void do_update() override { update(); }
};

struct AssetBrowserPanel : PanelI {
    AssetBrowserPanel(std::string_view name_, c8 icon_code_, bool open_ = true);

    void update(this AssetBrowserPanel &);

    void do_update() override { update(); }
};

struct ConsolePanel : PanelI {
    ConsolePanel(std::string_view name_, c8 icon_code_, bool open_ = true);

    void update(this ConsolePanel &);

    void do_update() override { update(); }
};

}  // namespace lr
