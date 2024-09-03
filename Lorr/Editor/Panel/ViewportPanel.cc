#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

namespace lr {
ViewportPanel::ViewportPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
}

void ViewportPanel::on_drop(this ViewportPanel &) {
    auto &app = EditorApp::get();
    auto &world = app.world;

    if (!world.active_scene) {
        LR_LOG_WARN("Trying to import asset into invalid scene!");
        return;
    }

    auto &scene = world.scene_at(world.active_scene.value());

    if (const auto *payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
        std::string_view path_sv(static_cast<const c8 *>(payload->Data), payload->DataSize);
        auto model_id = app.asset_man.load_model(path_sv);
        if (!model_id) {
            LR_LOG_ERROR("Failed to import model into the scene!");
            return;
        }

        auto model_entity = scene.create_entity("model").is_a<Prefab::RenderableModel>();
        auto *model_comp = model_entity.get_mut<Component::RenderableModel>();
        model_comp->model_id = static_cast<u32>(model_id.value());
    }
}

void ViewportPanel::update(this ViewportPanel &self) {
    auto &app = EditorApp::get();
    auto &world = app.world;
    auto &task_graph = app.world_render_pipeline.task_graph;
    auto &scene = world.scene_at(world.active_scene.value());
    auto camera = scene.active_camera->get_mut<Component::Camera>();
    auto camera_transform = scene.active_camera->get_mut<Component::Transform>();

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    auto work_area_size = ImGui::GetContentRegionAvail();
    auto &final_image = task_graph.task_image_at(app.world_render_pipeline.final_image);
    ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<iptr>(final_image.image_view_id)), work_area_size);

    if (ImGui::BeginDragDropTarget()) {
        self.on_drop();
        ImGui::EndDragDropTarget();
    }

    if (ImGui::IsWindowHovered()) {
        constexpr static f32 velocity = 3.0;
        bool reset_z = false;
        bool reset_x = false;

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            camera->velocity.z = velocity;
            reset_z |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            camera->velocity.z = -velocity;
            reset_z |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            camera->velocity.x = -velocity;
            reset_x |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            camera->velocity.x = velocity;
            reset_x |= true;
        }

        if (!reset_z) {
            camera->velocity.z = 0.0;
        }

        if (!reset_x) {
            camera->velocity.x = 0.0;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            auto drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0);
            camera_transform->rotation.x += drag.x * 0.1f;
            camera_transform->rotation.y -= drag.y * 0.1f;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    } else {
        camera->velocity = {};
    }

    ImGui::End();
}

}  // namespace lr
