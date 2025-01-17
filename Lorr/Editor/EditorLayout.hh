#pragma once

#include "Panel/Panels.hh"

#include "Engine/Asset/UUID.hh"
#include "Engine/Graphics/VulkanTypes.hh"

namespace lr {
enum class EditorTheme {
    Dark,
};

enum class ActiveTool : u32 {
    Cursor = 0,
    TerrainBrush,
    DecalBrush,
    MaterialEditor,
    World,
};

struct EditorLayout {
    ankerl::unordered_dense::map<std::string, UUID> editor_assets = {};

    std::vector<std::unique_ptr<PanelI>> panels = {};
    ls::option<u32> dockspace_id = ls::nullopt;
    bool show_profiler = false;
    bool show_assets = false;
    ActiveTool active_tool = ActiveTool::Cursor;

    void init(this EditorLayout &);
    auto destroy(this EditorLayout &) -> void;
    void setup_theme(this EditorLayout &, EditorTheme theme);
    void setup_dockspace(this EditorLayout &);
    void render(this EditorLayout &, vuk::Format format, vuk::Extent3D extent);

    template<typename T>
    std::pair<usize, T *> add_panel(this EditorLayout &self, std::string name, const c8 *icon, bool open = true) {
        usize panel_index = self.panels.size();
        auto icon_name = std::format("{}  {}", icon, name);
        auto panel = std::make_unique<T>(std::move(icon_name), open);
        T *panel_ptr = panel.get();
        self.panels.push_back(std::move(panel));
        return { panel_index, panel_ptr };
    }

    PanelI *panel_at(this EditorLayout &self, usize index) { return self.panels[index].get(); }
};
}  // namespace lr

namespace ImGuiLR {
bool drag_vec(i32 id, void *data, usize components, ImGuiDataType data_type);
void center_text(std::string_view str);
bool image_button(std::string_view text, ImTextureID texture_id, const ImVec2 &button_size);
}  // namespace ImGuiLR
