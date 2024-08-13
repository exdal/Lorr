#include "EditorApp.hh"

#include "Engine/Graphics/Task/BuiltinTasks.hh"

#include "Editor/Layout.hh"

namespace lr {
bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    self.task_graph.add_task<BuiltinTask::ImGuiTask>({
        .uses = {
            .attachment = self.swap_chain_image_id,
            .font = self.imgui_font_image_id,
        },
    });
    self.task_graph.present(self.swap_chain_image_id);
    self.layout.setup_theme(EditorTheme::Dark);

    return true;
}

bool EditorApp::update(this EditorApp &self, f32 delta_time) {
    ZoneScoped;

    self.layout.draw_base();

    return true;
}

}  // namespace lr
