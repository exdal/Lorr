#pragma once

#include "Panel/Panels.hh"

namespace lr {
enum class EditorTheme {
    Dark,
};

enum class ActiveTool : u32 {
    Cursor = 0,
    TerrainBrush,
    DecalBrush,
    MaterialEditor,
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
    std::pair<usize, T *> add_panel(this EditorLayout &self, std::string name, const c8 *icon, bool open = true) {
        usize panel_index = self.panels.size();
        auto icon_name = fmt::format("{}  {}", icon, name);
        auto panel = std::make_unique<T>(std::move(icon_name), open);
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
