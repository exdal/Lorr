#pragma once

#include <flecs.h>
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

struct SceneBrowserPanel : PanelI {
    flecs::entity selected_entity = {};

    SceneBrowserPanel(std::string name_, bool open_ = true);

    void update(this SceneBrowserPanel &);

    void do_update() override { update(); }
};

struct ViewportPanel : PanelI {
    u32 gizmo_op = 0;

    ViewportPanel(std::string name_, bool open_ = true);

    void on_drop(this ViewportPanel &);
    void update(this ViewportPanel &);

    void do_update() override { update(); }
    void do_project_refresh() override {}
};

struct InspectorPanel : PanelI {
    glm::vec2 sun_dir = {};

    InspectorPanel(std::string name_, bool open_ = true);

    void update(this InspectorPanel &);

    void do_update() override { update(); }
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
