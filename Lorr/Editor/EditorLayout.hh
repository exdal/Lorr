#pragma once

#include "Panel/Panels.hh"

namespace lr {
enum class EditorTheme {
    Dark,
};

struct EditorLayout {
    std::vector<std::unique_ptr<PanelI>> panels = {};
    ls::option<u32> dockspace_id = ls::nullopt;
    bool show_profiler = false;

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
