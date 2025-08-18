#include "Editor/Window/ViewportWindow.hh"

#include "Editor/EditorModule.hh"

#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Scene/ECSModule/Core.hh"
#include "Engine/Util/Icons/IconsMaterialDesignIcons.hh"

#include "Engine/Asset/Asset.hh"
#include "Engine/Core/App.hh"

#include <ImGuizmo.h>
#include <glm/gtx/matrix_decompose.hpp>

namespace led {
static auto on_drop(ViewportWindow &) -> void {
    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &editor = lr::App::mod<EditorModule>();
    auto &active_project = *editor.active_project;
    auto *active_scene = asset_man.get_scene(active_project.active_scene_uuid);

    if (const auto *asset_payload = ImGui::AcceptDragDropPayload("ASSET_BY_UUID")) {
        auto *uuid = static_cast<lr::UUID *>(asset_payload->Data);
        auto *asset = asset_man.get_asset(*uuid);
        switch (asset->type) {
            case lr::AssetType::Scene: {
                active_project.set_active_scene(*uuid);
            } break;
            case lr::AssetType::Model: {
                if (active_scene) {
                    active_scene->create_model_entity(*uuid);
                }
            } break;
            default:;
        }
    }
}

static auto draw_tools(ViewportWindow &self) -> void {
    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &editor = lr::App::mod<EditorModule>();
    auto &active_project = *editor.active_project;
    auto *active_scene = asset_man.get_scene(active_project.active_scene_uuid);

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
        editor.show_profiler = !editor.show_profiler;
        editor.frame_profiler.reset();
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Frame Profiler");
    }
    right_align_offset -= button_size.x;

    ImGui::SameLine(right_align_offset);
    if (ImGui::Button(ICON_MDI_BUG)) {
        editor.show_debug = !editor.show_debug;
    }
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("Debug");
    }
    right_align_offset -= button_size.x;
    ImGui::PopStyleColor(2);

    ImGui::SetNextWindowPos(editor_camera_popup_pos, ImGuiCond_Appearing);
    ImGui::SetNextWindowSize({ editor_camera_popup_width, 0 }, ImGuiCond_Appearing);
    if (ImGui::BeginPopup("editor_camera")) {
        ImGui::SeparatorText("Position");
        ImGui::drag_vec(0, glm::value_ptr(self.editor_camera.position), 3, ImGuiDataType_Float);

        ImGui::SeparatorText("Rotation");
        auto camera_rotation_degrees = glm::degrees(self.editor_camera.rotation);
        ImGui::drag_vec(1, glm::value_ptr(camera_rotation_degrees), 3, ImGuiDataType_Float);
        self.editor_camera.rotation = glm::radians(lr::Math::normalize_180(camera_rotation_degrees));

        ImGui::SeparatorText("FoV");
        ImGui::drag_vec(2, &self.editor_camera.fov, 1, ImGuiDataType_Float);

        ImGui::SeparatorText("Far Clip");
        ImGui::drag_vec(3, &self.editor_camera.far_clip, 1, ImGuiDataType_Float);

        ImGui::EndPopup();
    }

    if (editor.show_debug) {
        auto &cull_flags = reinterpret_cast<i32 &>(active_scene->get_cull_flags());
        ImGui::CheckboxFlags("Cull Meshlet Frustum", &cull_flags, std::to_underlying(lr::GPU::CullFlags::MeshletFrustum));
        ImGui::CheckboxFlags("Cull Triangle Back Face", &cull_flags, std::to_underlying(lr::GPU::CullFlags::TriangleBackFace));
        ImGui::CheckboxFlags("Cull Micro Triangles", &cull_flags, std::to_underlying(lr::GPU::CullFlags::MicroTriangles));
        ImGui::CheckboxFlags("Cull Occlusion", &cull_flags, std::to_underlying(lr::GPU::CullFlags::Occlusion));
        // ImGui::Checkbox("Debug Lines", &editor.scene_renderer.debug_lines);
    }
}

static auto draw_viewport(ViewportWindow &self, vuk::Format format, vuk::Extent3D) -> void {
    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &editor = lr::App::mod<EditorModule>();
    auto &scene_renderer = lr::App::mod<lr::SceneRenderer>();
    auto &imgui_renderer = lr::App::mod<lr::ImGuiRenderer>();

    auto &active_project = *editor.active_project;
    auto *active_scene = asset_man.get_scene(active_project.active_scene_uuid);
    auto &selected_entity = active_project.selected_entity;

    auto *current_window = ImGui::GetCurrentWindow();
    auto window_rect = current_window->InnerRect;
    auto window_pos = window_rect.Min;
    auto window_size = window_rect.GetSize();
    auto work_area_size = ImGui::GetContentRegionAvail();
    auto &io = ImGui::GetIO();

    ImGuizmo::SetDrawlist();
    ImGuizmo::SetOrthographic(false);
    ImGuizmo::SetRect(window_pos.x, window_pos.y, window_size.x, window_size.y);
    ImGuizmo::AllowAxisFlip(false);
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

    self.editor_camera.resolution = { window_size.x, window_size.y };
    auto prepared_frame = active_scene->prepare_frame(scene_renderer, self.editor_camera); // NOLINT(cppcoreguidelines-slicing)

    auto viewport_attachment_info = vuk::ImageAttachment{
        .image_type = vuk::ImageType::e2D,
        .usage = vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        .extent = { .width = static_cast<u32>(window_size.x), .height = static_cast<u32>(window_size.y), .depth = 1 },
        .format = format,
        .sample_count = vuk::Samples::e1,
        .view_type = vuk::ImageViewType::e2D,
        .level_count = 1,
        .layer_count = 1,
    };
    auto viewport_attachment = vuk::declare_ia("viewport", viewport_attachment_info);
    auto scene_render_info = lr::SceneRenderInfo{
        .delta_time = ImGui::GetIO().DeltaTime,
        .cull_flags = active_scene->get_cull_flags(),
        .picking_texel = requested_texel_transform,
    };
    viewport_attachment = scene_renderer.render(std::move(viewport_attachment), scene_render_info, prepared_frame);
    auto scene_render_image_idx = imgui_renderer.add_image(std::move(viewport_attachment));
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
        auto camera_forward = glm::vec3(0.0, 0.0, 1.0) * lr::Math::quat_dir(self.editor_camera.rotation);
        auto camera_projection = glm::perspective(
            glm::radians(self.editor_camera.fov),
            self.editor_camera.aspect_ratio(),
            self.editor_camera.far_clip,
            self.editor_camera.near_clip
        );
        auto camera_view = glm::lookAt(self.editor_camera.position, self.editor_camera.position + camera_forward, glm::vec3(0.0, 1.0, 0.0));
        camera_projection[1][1] *= -1.0f;

        auto *transform = selected_entity.get_mut<lr::ECS::Transform>();
        auto T = glm::translate(glm::mat4(1.0), transform->position);
        auto R = glm::mat4_cast(glm::quat(transform->rotation));
        auto S = glm::scale(glm::mat4(1.0), transform->scale);
        auto gizmo_mat = T * R * S;
        auto delta_mat = glm::mat4(1.0f);

        ImGuizmo::Manipulate(
            glm::value_ptr(camera_view),
            glm::value_ptr(camera_projection),
            static_cast<ImGuizmo::OPERATION>(self.gizmo_op),
            ImGuizmo::MODE::WORLD,
            glm::value_ptr(gizmo_mat),
            glm::value_ptr(delta_mat)
        );

        if (ImGuizmo::IsUsing()) {
            glm::vec3 scale = {};
            glm::quat rotation = {};
            glm::vec3 position = {};
            glm::vec3 skew = {};
            glm::vec4 perspective = {};
            glm::decompose(delta_mat, scale, rotation, position, skew, perspective);

            if (self.gizmo_op == ImGuizmo::TRANSLATE) {
                transform->position += position;
            } else if (self.gizmo_op == ImGuizmo::ROTATE) {
                transform->rotation += glm::eulerAngles(glm::quat(rotation[3], rotation[0], rotation[1], rotation[2]));
            } else if (self.gizmo_op == ImGuizmo::SCALE) {
                transform->scale *= scale;
            }

            selected_entity.modified<lr::ECS::Transform>();
        }
    }

    //  ── CAMERA CONTROLLER ───────────────────────────────────────────────
    if (!ImGuizmo::IsUsingAny() && ImGui::IsWindowHovered()) {
        constexpr auto EDITOR_CAMERA_VELOCITY = 2.0f;
        if (ImGui::IsKeyDown(ImGuiKey_W)) {
            self.editor_camera.velocity.z = -EDITOR_CAMERA_VELOCITY;
        }

        if (ImGui::IsKeyDown(ImGuiKey_S)) {
            self.editor_camera.velocity.z = EDITOR_CAMERA_VELOCITY;
        }

        if (ImGui::IsKeyDown(ImGuiKey_A)) {
            self.editor_camera.velocity.x = -EDITOR_CAMERA_VELOCITY;
        }

        if (ImGui::IsKeyDown(ImGuiKey_D)) {
            self.editor_camera.velocity.x = EDITOR_CAMERA_VELOCITY;
        }

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Right)) {
            auto drag = ImGui::GetMouseDragDelta(ImGuiMouseButton_Right, 0);
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Right);

            auto sensitivity = 0.1f;
            auto camera_rotation_degrees = glm::degrees(self.editor_camera.rotation);
            camera_rotation_degrees.x += drag.x * sensitivity;
            camera_rotation_degrees.y += drag.y * sensitivity;
            self.editor_camera.rotation = glm::radians(camera_rotation_degrees);
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
    auto &editor = lr::App::mod<EditorModule>();
    const auto should_render = editor.active_project && editor.active_project->active_scene_uuid;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0, 0.0));
    if (ImGui::Begin(self.name.data())) {
        if (should_render) {
            draw_viewport(self, format, extent);
            draw_tools(self);
        }

        auto *current_window = ImGui::GetCurrentWindow();
        if (editor.active_project && ImGui::BeginDragDropTargetCustom(current_window->InnerRect, ImGui::GetID("##viewport_drop_target"))) {
            on_drop(self);
            ImGui::EndDragDropTarget();
        }
    }

    ImGui::End();
    ImGui::PopStyleVar();
}
} // namespace led
