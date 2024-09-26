#pragma once

#include <flecs.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

namespace lr {
struct Directory {
    fs::path path = {};
    std::vector<Directory> subdirs = {};
    std::vector<fs::path> files = {};
};

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

struct ToolsPanel : PanelI {
    ToolsPanel(std::string name_, bool open_ = true);

    void update(this ToolsPanel &);

    void do_update() override { update(); }
};

struct SceneBrowserPanel : PanelI {
    flecs::entity selected_entity = {};

    SceneBrowserPanel(std::string name_, bool open_ = true);

    void update(this SceneBrowserPanel &);

    void do_update() override { update(); }
};

struct ViewportPanel : PanelI {
    ls::option<flecs::entity> camera_entity = ls::nullopt;

    ViewportPanel(std::string name_, bool open_ = true);

    void on_drop(this ViewportPanel &);
    void on_project_refresh(this ViewportPanel &);
    void update(this ViewportPanel &);

    void do_update() override { update(); }
    void do_project_refresh() override { on_project_refresh(); }
};

struct InspectorPanel : PanelI {
    glm::vec2 sun_dir = {};

    InspectorPanel(std::string name_, bool open_ = true);

    void update(this InspectorPanel &);

    void do_update() override { update(); }
};

struct AssetBrowserPanel : PanelI {
    ls::option<Directory> asset_dir = ls::nullopt;
    Directory *selected_dir = nullptr;

    AssetBrowserPanel(std::string name_, bool open_ = true);

    void on_project_refresh(this AssetBrowserPanel &);
    void refresh_file_tree(this AssetBrowserPanel &);

    void draw_project_tree(this AssetBrowserPanel &);
    void draw_dir_contents(this AssetBrowserPanel &);
    void draw_file_tree(this AssetBrowserPanel &, Directory &root_dir);

    void update(this AssetBrowserPanel &);

    void do_update() override { update(); }
    void do_project_refresh() override { on_project_refresh(); }
};

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
