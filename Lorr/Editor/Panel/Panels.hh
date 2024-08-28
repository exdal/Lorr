#pragma once

#include "Engine/World/Scene.hh"

#include <imgui.h>
#include <imgui_internal.h>

namespace lr {
struct Directory {
    fs::path path = {};
    std::vector<Directory> subdirs = {};
    std::vector<fs::path> files = {};
};

struct PanelI {
    std::string_view name = {};
    bool open = true;

    PanelI(std::string_view name_, bool open_ = true)
        : name(name_),
          open(open_) {};

    virtual ~PanelI() = default;
    virtual void do_update() = 0;
};

struct ToolsPanel : PanelI {
    ToolsPanel(std::string_view name_, bool open_ = true);

    void update(this ToolsPanel &);

    void do_update() override { update(); }
};

struct SceneBrowserPanel : PanelI {
    flecs::entity selected_entity = {};

    SceneBrowserPanel(std::string_view name_, bool open_ = true);

    void update(this SceneBrowserPanel &);

    void do_update() override { update(); }
};

struct ViewportPanel : PanelI {
    ViewportPanel(std::string_view name_, bool open_ = true);

    void update(this ViewportPanel &);

    void do_update() override { update(); }
};

struct InspectorPanel : PanelI {
    glm::vec2 sun_dir = {};

    InspectorPanel(std::string_view name_, bool open_ = true);

    void update(this InspectorPanel &);

    void do_update() override { update(); }
};

struct AssetBrowserPanel : PanelI {
    Directory asset_dir = {};

    AssetBrowserPanel(std::string_view name_, bool open_ = true);

    void refresh_file_tree(this AssetBrowserPanel &);

    void draw_project_tree(this AssetBrowserPanel &);
    void draw_file_tree(this AssetBrowserPanel &, Directory &root_dir);

    void update(this AssetBrowserPanel &);

    void do_update() override { update(); }
};

struct ConsolePanel : PanelI {
    ConsolePanel(std::string_view name_, bool open_ = true);

    void update(this ConsolePanel &);

    void do_update() override { update(); }
};

}  // namespace lr
