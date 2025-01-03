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
};
}  // namespace lr
