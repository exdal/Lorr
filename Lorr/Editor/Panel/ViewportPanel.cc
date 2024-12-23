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

    this->gizmo_op = ImGuizmo::TRANSLATE;
}

void ViewportPanel::on_drop(this ViewportPanel &) {
    auto &app = EditorApp::get();
    auto &asset_man = app.asset_man;
    auto &world = app.world;
    auto &world_renderer = app.world_renderer;

    if (const auto *asset_payload = ImGui::AcceptDragDropPayload("ASSET_BY_UUID")) {
        auto *uuid = static_cast<UUID *>(asset_payload->Data);
        auto *asset = asset_man.get_asset(*uuid);
        switch (asset->type) {
            case AssetType::Scene: {
                if (world.active_scene().has_value()) {
                    // asset_man.unload_scene(world.active_scene().value());
                }
                asset_man.load_scene(asset);
                world.set_active_scene(asset->scene_id);
                auto &pbr_flags = world_renderer.get_pbr_flags();
                pbr_flags.render_editor_grid = true;
                pbr_flags.render_sky = true;
                pbr_flags.render_aerial_perspective = true;
                world_renderer.update_pbr_graph();
                break;
            }
            case AssetType::None:
            case AssetType::Shader:
            case AssetType::Model:
            case AssetType::Texture:
            case AssetType::Material:
            case AssetType::Font:
                break;
        }
    }
}

void ViewportPanel::update(this ViewportPanel &self) {
    auto &app = EditorApp::get();
    auto &asset_man = app.asset_man;
    auto &world_renderer = app.world_renderer;
    auto &world = app.world;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    auto *current_window = ImGui::GetCurrentWindow();
    auto editor_image = world_renderer.composition_image();

    if (world.active_scene().has_value() && editor_image) {
        auto scene = asset_man.get_scene(world.active_scene().value());
        auto editor_camera = scene.editor_camera();
        auto window_rect = current_window->InnerRect;
        auto window_pos = window_rect.Min;
        auto window_size = window_rect.GetSize();
        auto work_area_size = ImGui::GetContentRegionAvail();

        ImGuizmo::SetOrthographic(false);
        ImGuizmo::SetDrawlist();
        ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);

        ImGui::Image(reinterpret_cast<ImTextureID>(static_cast<iptr>(editor_image.id())), work_area_size);

        auto *camera = editor_camera.get_mut<Component::Camera>();
        auto *camera_transform = editor_camera.get_mut<Component::Transform>();

        auto query = world.ecs()
                         .query_builder<Component::EditorSelected, Component::Transform>()  //
                         .with(flecs::ChildOf, scene.root())
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
                    static_cast<ImGuizmo::OPERATION>(self.gizmo_op),
                    ImGuizmo::MODE::LOCAL,
                    gizmo_mat)) {
                ImGuizmo::DecomposeMatrixToComponents(gizmo_mat, &t.position[0], &t.rotation[0], &t.scale[0]);
            }
        });

        if (!ImGuizmo::IsUsingAny() && ImGui::IsWindowHovered()) {
            bool reset_z = false;
            bool reset_x = false;

            if (ImGui::IsKeyDown(ImGuiKey_W)) {
                camera->cur_axis_velocity.z = camera->velocity;
                reset_z |= true;
            }

            if (ImGui::IsKeyDown(ImGuiKey_S)) {
                camera->cur_axis_velocity.z = -camera->velocity;
                reset_z |= true;
            }

            if (ImGui::IsKeyDown(ImGuiKey_A)) {
                camera->cur_axis_velocity.x = -camera->velocity;
                reset_x |= true;
            }

            if (ImGui::IsKeyDown(ImGuiKey_D)) {
                camera->cur_axis_velocity.x = camera->velocity;
                reset_x |= true;
            }

            if (!reset_z) {
                camera->cur_axis_velocity.z = 0.0;
            }

            if (!reset_x) {
                camera->cur_axis_velocity.x = 0.0;
            }

            if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
                auto drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0);
                camera_transform->rotation.x += drag.x * 0.1f;
                camera_transform->rotation.y -= drag.y * 0.1f;
                ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
            }
        } else {
            camera->cur_axis_velocity = {};
        }

        //  ── VIEWPORT TOOLS ──────────────────────────────────────────────────
        auto frame_spacing = ImGui::GetFrameHeight();
        auto frame_padding = ImGui::GetStyle().FramePadding;
        auto button_size = ImVec2(frame_spacing, frame_spacing);
        auto tools_frame_min = ImVec2(window_pos.x + frame_padding.x, window_pos.y + frame_padding.y);
        auto tools_frame_max = ImVec2(window_rect.Max.x - frame_padding.y, tools_frame_min.y + button_size.y);
        float rounding = 5.0;

        ImGui::RenderFrame(tools_frame_min, tools_frame_max, ImGui::GetColorU32(ImVec4(0.0, 0.0, 0.0, 0.3)), false, rounding);
        ImGui::SetCursorPos(ImVec2(frame_padding.x + rounding, frame_padding.y + button_size.y));

        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.082f, 0.082f, 0.082f, 1.00f));
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0, 0.0, 0.0, 0.00));

        if (ImGui::Button(Icon::fa::up_down_left_right, button_size)) {
            self.gizmo_op = ImGuizmo::TRANSLATE;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Translate");
        }

        ImGui::SameLine();
        if (ImGui::Button(Icon::fa::arrows_rotate, button_size)) {
            self.gizmo_op = ImGuizmo::ROTATE;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Rotate");
        }

        ImGui::SameLine();
        if (ImGui::Button(Icon::fa::up_right_and_down_left_from_center)) {
            self.gizmo_op = ImGuizmo::SCALE;
        }
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
            ImGui::SetTooltip("Scale");
        }

        ImGui::PopStyleColor(2);
    } else {
        lg::center_text("No scene to render.");
    }

    if (ImGui::BeginDragDropTargetCustom(current_window->InnerRect, ImGui::GetID("##viewport_drop_target"))) {
        self.on_drop();
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}

}  // namespace lr
