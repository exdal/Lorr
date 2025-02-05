#pragma once

#include "Engine/Graphics/Vulkan.hh"

#include <imgui.h>

namespace lr {
struct ImGuiRenderer {
    Device *device = nullptr;
    Image font_atlas_image = {};
    ImageView font_atlas_view = {};
    Pipeline pipeline = {};
    std::vector<vuk::Value<vuk::ImageAttachment>> rendering_attachments = {};

    auto init(this ImGuiRenderer &, Device *device) -> void;
    auto add_image(this ImGuiRenderer &, vuk::Value<vuk::ImageAttachment> &&attachment) -> ImTextureID;
    auto add_image(this ImGuiRenderer &, ImageView &image_view) -> ImTextureID;

    auto begin_frame(this ImGuiRenderer &, f64 delta_time, const vuk::Extent3D &extent) -> void;
    auto end_frame(this ImGuiRenderer &, vuk::Value<vuk::ImageAttachment> &&attachment) -> vuk::Value<vuk::ImageAttachment>;

    auto on_mouse_pos(this ImGuiRenderer &, glm::vec2 pos) -> void;
    auto on_mouse_button(this ImGuiRenderer &, u8 button, bool down) -> void;
    auto on_mouse_scroll(this ImGuiRenderer &, glm::vec2 offset) -> void;
    auto on_key(this ImGuiRenderer &, u32 key_code, u32 scan_code, u16 mods, bool down) -> void;
    auto on_text_input(this ImGuiRenderer &, const c8 *text) -> void;
};
}  // namespace lr
