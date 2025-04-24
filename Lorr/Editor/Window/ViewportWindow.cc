#include "Editor/Window/ViewportWindow.hh"

#include "Editor/EditorApp.hh"

#include "Engine/Scene/ECSModule/Core.hh"
#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

#include <ImGuizmo.h>
#include <glm/gtx/quaternion.hpp>

namespace led {
static auto on_drop(ViewportWindow &) -> void {
    auto &app = EditorApp::get();
    auto &active_project = *app.active_project;

    if (const auto *asset_payload = ImGui::AcceptDragDropPayload("ASSET_BY_UUID")) {
        auto *uuid = static_cast<lr::UUID *>(asset_payload->Data);
        auto *asset = app.asset_man.get_asset(*uuid);
        switch (asset->type) {
            case lr::AssetType::Scene: {
                active_project.set_active_scene(*uuid);
            } break;
            default:;
        }
    }
}

static auto draw_tools(ViewportWindow &self) -> void {
    auto &app = EditorApp::get();
    auto &active_project = *app.active_project;
    auto *active_scene = app.asset_man.get_scene(active_project.active_scene_uuid);

    auto *current_window = ImGui::GetCurrentWindow();
    auto window_rect = current_window->InnerRect;
    auto window_pos = window_rect.Min;
    auto work_area_size = ImGui::GetContentRegionAvail();

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

    if (ImGui::Button(ICON_MDI_AXIS_ARROW, button_size)) {
        self.gizmo_op = ImGuizmo::TRANSLATE;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Translate");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_ROTATE_360, button_size)) {
        self.gizmo_op = ImGuizmo::ROTATE;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Rotate");
    }

    ImGui::SameLine();
    if (ImGui::Button(ICON_MDI_ARROW_EXPAND)) {
        self.gizmo_op = ImGuizmo::SCALE;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Scale");
    }

    f32 right_align_offset = work_area_size.x - rounding - frame_padding.x - button_size.x;
    ImGui::SameLine(right_align_offset);
    if (ImGui::Button(ICON_MDI_CAMERA)) {
        ImGui::OpenPopup("editor_camera");
    }
    const static f32 editor_camera_popup_width = 200.0f;
    auto editor_camera_popup_pos = ImVec2({});
    {
        auto cur_button_pos = ImGui::GetItemRectMin();
        editor_camera_popup_pos.x = cur_button_pos.x + (button_size.x - editor_camera_popup_width) * 0.5f;
        editor_camera_popup_pos.y = cur_button_pos.y + button_size.y;
    }

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Editor Camera Settings");
    }
    right_align_offset -= button_size.x;

    ImGui::SameLine(right_align_offset);
    if (ImGui::Button(ICON_MDI_CHART_BAR)) {
        // app.layout.show_profiler = !app.layout.show_profiler;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Frame Profiler");
    }
    right_align_offset -= button_size.x;

    ImGui::PopStyleColor(2);

    ImGui::SetNextWindowPos(editor_camera_popup_pos);
    ImGui::SetNextWindowSize({ editor_camera_popup_width, 0 });
    if (ImGui::BeginPopup("editor_camera")) {
        auto max_widget_width = editor_camera_popup_width - frame_padding.x * 2;
        auto editor_camera = active_scene->get_editor_camera();
        auto *camera_transform = editor_camera.get_mut<lr::ECS::Transform>();
        auto *camera_info = editor_camera.get_mut<lr::ECS::Camera>();

        ImGui::SeparatorText("Position");
        ImGui::drag_vec(0, glm::value_ptr(camera_transform->position), 3, ImGuiDataType_Float);
        ImGui::SeparatorText("Rotation");
        ImGui::drag_vec(1, glm::value_ptr(camera_transform->rotation), 3, ImGuiDataType_Float);
        ImGui::SeparatorText("FoV");
        ImGui::drag_vec(2, &camera_info->fov, 1, ImGuiDataType_Float);
        ImGui::SeparatorText("Far Clip");
        ImGui::drag_vec(3, &camera_info->far_clip, 1, ImGuiDataType_Float);
        ImGui::SeparatorText("Velocity");
        ImGui::drag_vec(4, &camera_info->velocity_mul, 1, ImGuiDataType_Float);
        ImGui::SeparatorText("Culling");
        ImGui::Checkbox("Freeze frustum", &camera_info->freeze_frustum);
        auto &cull_flags = reinterpret_cast<i32 &>(active_scene->get_cull_flags());
        ImGui::CheckboxFlags("Cull Meshlet Frustum", &cull_flags, std::to_underlying(lr::GPU::CullFlags::MeshletFrustum));
        ImGui::CheckboxFlags("Cull Triangle Back Face", &cull_flags, std::to_underlying(lr::GPU::CullFlags::TriangleBackFace));
        ImGui::CheckboxFlags("Cull Micro Triangles", &cull_flags, std::to_underlying(lr::GPU::CullFlags::MicroTriangles));

        ImGui::SeparatorText("Debug View");
        ImGui::SetNextItemWidth(max_widget_width);
        constexpr static const c8 *debug_views_str[] = {
            "None", "Triangles", "Meshlets", "Overdraw", "Albedo", "Normal", "Emissive", "Metallic", "Roughness", "Occlusion",
        };
        static_assert(ls::count_of(debug_views_str) == std::to_underlying(lr::GPU::DebugView::Count));

        auto debug_view_idx = reinterpret_cast<std::underlying_type_t<lr::GPU::DebugView> *>(&app.scene_renderer.debug_view);
        const auto preview_str = debug_views_str[*debug_view_idx];
        if (ImGui::BeginCombo("", preview_str)) {
            for (i32 i = 0; i < static_cast<i32>(ls::count_of(debug_views_str)); i++) {
                const auto is_selected = i == *debug_view_idx;
                if (ImGui::Selectable(debug_views_str[i], is_selected)) {
                    *debug_view_idx = i;
                }

                if (is_selected) {
                    ImGui::SetItemDefaultFocus();
                }
            }

            ImGui::EndCombo();
        }

        ImGui::SetNextItemWidth(max_widget_width);
        ImGui::DragFloat("Heatmap scale", &app.scene_renderer.debug_heatmap_scale);
        ImGui::EndPopup();
    }
}

static auto draw_viewport(ViewportWindow &self, vuk::Format format, vuk::Extent3D) -> void {
    auto &app = EditorApp::get();
    auto &active_project = *app.active_project;
    auto *active_scene = app.asset_man.get_scene(active_project.active_scene_uuid);
    auto &selected_entity = active_project.selected_entity;

    auto editor_camera = active_scene->get_editor_camera();
    auto *current_window = ImGui::GetCurrentWindow();
    auto window_rect = current_window->InnerRect;
    auto window_pos = window_rect.Min;
    auto window_size = window_rect.GetSize();
    auto work_area_size = ImGui::GetContentRegionAvail();
    auto &io = ImGui::GetIO();

    auto *camera = editor_camera.get_mut<lr::ECS::Camera>();
    auto *camera_transform = editor_camera.get_mut<lr::ECS::Transform>();
    camera->aspect_ratio = window_size.x / window_size.y;

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);
    ImGuizmo::AllowAxisFlip(true);
    ImGuizmo::Enable(true);
    ImGuizmo::PushID(1); // surely thats unique enough
    LS_DEFER() {
        ImGuizmo::PopID();
    };

    ls::option<glm::uvec2> requested_texel_transform = ls::nullopt;
    const auto dragging = io.MouseDragMaxDistanceSqr[0] >= (io.MouseDragThreshold * io.MouseDragThreshold);
    const auto update_visible_entity = ImGui::IsWindowHovered() && !dragging && ImGui::IsMouseReleased(0);
    if (update_visible_entity) {
        auto mouse_pos = ImGui::GetMousePos();
        auto mouse_pos_rel = ImVec2(mouse_pos.x - window_pos.x, mouse_pos.y - window_pos.y);
        requested_texel_transform.emplace(glm::uvec2(mouse_pos_rel.x, mouse_pos_rel.y));
    }
    auto scene_render_info = lr::SceneRenderInfo{
        .format = format,
        .extent = vuk::Extent3D(static_cast<u32>(window_size.x), static_cast<u32>(window_size.y), 1),
        .picking_texel = requested_texel_transform,
    };
    auto scene_render_result = active_scene->render(app.scene_renderer, scene_render_info);
    auto scene_render_image_idx = app.imgui_renderer.add_image(std::move(scene_render_result));
    ImGui::Image(scene_render_image_idx, work_area_size);

    if (update_visible_entity) {
        if (scene_render_info.picked_transform_index.has_value()) {
            auto picked_entity = active_scene->find_entity(scene_render_info.picked_transform_index.value());
            if (picked_entity) {
                selected_entity = picked_entity;
            }
        } else {
            selected_entity = {};
        }
    }

    if (selected_entity && selected_entity.has<lr::ECS::Transform>()) {
        auto camera_forward = glm::vec3(0.0, 0.0, -1.0) * lr::Math::compose_quat(glm::radians(camera_transform->rotation));
        auto camera_projection = glm::perspectiveLH_NO(glm::radians(camera->fov), camera->aspect_ratio, camera->far_clip, camera->near_clip);
        auto camera_view = glm::lookAt(camera_transform->position, camera_transform->position + camera_forward, glm::vec3(0.0, -1.0, 0.0));

        auto *transform = selected_entity.get_mut<lr::ECS::Transform>();
        const auto &rotation = glm::radians(transform->rotation);
        auto gizmo_mat = glm::translate(glm::mat4(1.0), transform->position);
        gizmo_mat *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
        gizmo_mat *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
        gizmo_mat *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
        gizmo_mat *= glm::scale(glm::mat4(1.0), transform->scale);

        if (ImGuizmo::Manipulate(
                glm::value_ptr(camera_view), //
                glm::value_ptr(camera_projection),
                static_cast<ImGuizmo::OPERATION>(self.gizmo_op),
                ImGuizmo::MODE::WORLD,
                glm::value_ptr(gizmo_mat)
            ))
        {
            ImGuizmo::DecomposeMatrixToComponents( //
                glm::value_ptr(gizmo_mat),
                glm::value_ptr(transform->position),
                glm::value_ptr(transform->rotation),
                glm::value_ptr(transform->scale)
            );

            selected_entity.modified<lr::ECS::Transform>();
        }
    }

    //  ── CAMERA CONTROLLER ───────────────────────────────────────────────
    if (!ImGuizmo::IsUsingAny() && ImGui::IsWindowHovered()) {
        bool reset_z = false;
        bool reset_x = false;

        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            camera->axis_velocity.z = -camera->velocity_mul;
            reset_z |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            camera->axis_velocity.z = camera->velocity_mul;
            reset_z |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            camera->axis_velocity.x = -camera->velocity_mul;
            reset_x |= true;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            camera->axis_velocity.x = camera->velocity_mul;
            reset_x |= true;
        }

        if (!reset_z) {
            camera->axis_velocity.z = 0.0;
        }

        if (!reset_x) {
            camera->axis_velocity.x = 0.0;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
            auto drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left, 0);
            camera_transform->rotation.x += drag.x * 0.1f;
            camera_transform->rotation.y += drag.y * 0.1f;
            camera_transform->rotation = lr::Math::normalize_180(camera_transform->rotation);

            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
        }
    }
}

ViewportWindow::ViewportWindow(std::string name_, bool open_) : IWindow(std::move(name_), open_) {
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

    this->gizmo_op = ImGuizmo::TRANSLATE;
}

auto ViewportWindow::render(this ViewportWindow &self, vuk::Format format, vuk::Extent3D extent) -> void {
    auto &app = EditorApp::get();
    const auto should_render = app.active_project && app.active_project->active_scene_uuid;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    ImGui::Begin(self.name.data());
    ImGui::PopStyleVar();

    if (should_render) {
        draw_viewport(self, format, extent);
        draw_tools(self);
    }

    auto *current_window = ImGui::GetCurrentWindow();
    if (app.active_project && ImGui::BeginDragDropTargetCustom(current_window->InnerRect, ImGui::GetID("##viewport_drop_target"))) {
        on_drop(self);
        ImGui::EndDragDropTarget();
    }

    ImGui::End();
}
} // namespace led
