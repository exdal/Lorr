#include "Panels.hh"

#include "EditorApp.hh"

#include "Engine/World/Components.hh"

#include <ImGuizmo.h>

namespace lr {
ViewportPanel::ViewportPanel(std::string name_, bool open_)
    : PanelI(std::move(name_), open_) {
    const ImVec4 inspector_color_x = ImVec4(0.75f, 0.20f, 0.20f, 0.80f);
    const ImVec4 inspector_color_y = ImVec4(0.20f, 0.75f, 0.20f, 0.80f);
    const ImVec4 inspector_color_z = ImVec4(0.20f, 0.20f, 0.75f, 0.80f);

    ImGuizmo::Style &style = ImGuizmo::GetStyle();
    style.Colors[ImGuizmo::COLOR::DIRECTION_X] = ImVec4(inspector_color_x.x, inspector_color_x.y, inspector_color_x.z, 1.0f);
    style.Colors[ImGuizmo::COLOR::DIRECTION_Y] = ImVec4(inspector_color_y.x, inspector_color_y.y, inspector_color_y.z, 1.0f);
    style.Colors[ImGuizmo::COLOR::DIRECTION_Z] = ImVec4(inspector_color_z.x, inspector_color_z.y, inspector_color_z.z, 1.0f);
    style.Colors[ImGuizmo::COLOR::PLANE_X] = inspector_color_x;
    style.Colors[ImGuizmo::COLOR::PLANE_Y] = inspector_color_y;
    style.Colors[ImGuizmo::COLOR::PLANE_Z] = inspector_color_z;
    style.Colors[ImGuizmo::COLOR::HATCHED_AXIS_LINES] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

    style.CenterCircleSize = 5.0f;
    style.TranslationLineThickness = 4.0f;
    style.TranslationLineArrowSize = 6.0f;
    style.RotationLineThickness = 3.0f;
    style.RotationOuterLineThickness = 2.0f;
    style.ScaleLineThickness = 4.0f;
    style.ScaleLineCircleSize = 7.0f;
    ImGuizmo::AllowAxisFlip(true);
}

void ViewportPanel::on_drop(this ViewportPanel &self) {
    auto &app = EditorApp::get();
    auto &world = app.world;

    if (!world.active_scene.has_value()) {
        return;
    }

    auto &scene = world.scene_at(world.active_scene.value());
    if (const auto *asset_payload = ImGui::AcceptDragDropPayload("ASSET_PATH")) {
        std::string_view path_sv(static_cast<const c8 *>(asset_payload->Data), asset_payload->DataSize);
        auto model_ident = Identifier::random();
        auto model = app.asset_man.load_model(model_ident, path_sv);
        if (!model) {
            LOG_ERROR("Failed to import model into the scene!");
            return;
        }

        scene
            .create_entity(model_ident.sv())  //
            .set<Component::Transform>({})
            .set<Component::RenderableModel>({ model_ident });
        world.renderer->construct_scene();
    } else if (const auto *camera_payload = ImGui::AcceptDragDropPayload("ATTACH_CAMERA")) {
        auto camera_id = *static_cast<flecs::entity_t *>(camera_payload->Data);
        self.camera_entity = world.ecs.entity(camera_id);
    }
}

void ViewportPanel::on_project_refresh(this ViewportPanel &self) {
    self.camera_entity.reset();
}

void ViewportPanel::update(this ViewportPanel &self) {
    auto &app = EditorApp::get();
    auto &world = app.world;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    auto *current_window = ImGui::GetCurrentWindow();
    auto window_rect = current_window->InnerRect;
    auto window_pos = window_rect.Min;
    auto window_size = window_rect.GetSize();
    auto work_area_size = ImGui::GetContentRegionAvail();

    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);

    if (self.camera_entity.has_value()) {
        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<iptr>(world.renderer->final_image)), work_area_size);

        auto *camera = self.camera_entity->get_mut<Component::Camera>();
        auto *camera_transform = self.camera_entity->get_mut<Component::Transform>();

        auto query = app.world.ecs
                         .query_builder<Component::EditorSelected, Component::Transform>()  //
                         .without<Component::Camera>()
                         .build();
        query.each([&](Component::EditorSelected, Component::Transform &t) {
            auto projection = camera->projection;
            projection[1][1] *= -1;

            f32 gizmo_mat[16] = {};
            ImGuizmo::RecomposeMatrixFromComponents(
                glm::value_ptr(t.position), glm::value_ptr(t.rotation), glm::value_ptr(t.scale), gizmo_mat);
            if (ImGuizmo::Manipulate(
                    glm::value_ptr(camera_transform->matrix),  //
                    glm::value_ptr(projection),
                    ImGuizmo::TRANSLATE | ImGuizmo::ROTATE | ImGuizmo::SCALE,
                    ImGuizmo::MODE::LOCAL,
                    gizmo_mat)) {
                ImGuizmo::DecomposeMatrixToComponents(gizmo_mat, &t.position[0], &t.rotation[0], &t.scale[0]);
            }
        });

        if (!ImGuizmo::IsUsingAny() && ImGui::IsWindowHovered()) {
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
    } else {
        lg::center_text("Drop and Drop a camera entity to attach this viewport");
    }

    if (ImGui::BeginDragDropTargetCustom(current_window->InnerRect, ImGui::GetID("##viewport_drop_target"))) {
        self.on_drop();
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

}  // namespace lr
