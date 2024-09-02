#pragma once

#include "Panel/Panels.hh"

#define LRED_ICON_ASSETS "\uf87c"
#define LRED_ICON_INFO "\uf05a"
#define LRED_ICON_WRENCH "\uf0ad"
#define LRED_ICON_SANDWICH "\uf0c9"
#define LRED_ICON_EYE "\uf06e"
#define LRED_ICON_CURSOR "\uf245"
#define LRED_ICON_BRUSH "\uf1fc"
#define LRED_ICON_SAPLING "\uf4d8"
#define LRED_ICON_MEASURE "\uf545"
#define LRED_ICON_EGG "\uf7fb"
#define LRED_ICON_EARTH "\uf57d"
#define LRED_ICON_SUN "\uf185"

namespace lr {
enum class EditorTheme {
    Dark,
};

enum class ActiveTool : u32 {
    Cursor = 0,
    Atmosphere,
};

struct EditorLayout {
    std::vector<std::unique_ptr<PanelI>> panels = {};
    ls::option<u32> dockspace_id = ls::nullopt;
    bool show_profiler = false;
    ActiveTool active_tool = ActiveTool::Cursor;

    void init(this EditorLayout &);
    void setup_theme(this EditorLayout &, EditorTheme theme);
    void setup_dockspace(this EditorLayout &);
    void update(this EditorLayout &);

    template<typename T>
    std::pair<usize, T *> add_panel(this EditorLayout &self, std::string_view name, bool open = true) {
        usize panel_index = self.panels.size();
        auto panel = std::make_unique<T>(name, open);
        T *panel_ptr = panel.get();
        self.panels.push_back(std::move(panel));
        return { panel_index, panel_ptr };
    }

    PanelI *panel_at(this EditorLayout &self, usize index) { return self.panels[index].get(); }
};
}  // namespace lr

namespace LRGui {
bool DragXY(glm::vec2 &coords);
bool DragXYZ(glm::vec3 &coords);
}  // namespace LRGui
