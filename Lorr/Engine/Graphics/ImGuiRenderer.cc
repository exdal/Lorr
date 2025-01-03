#include "Engine/Graphics/ImGuiRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto ImGuiRenderer::init(this ImGuiRenderer &self, Device *device) -> void {
    ZoneScoped;

    self.device = device;

    auto &app = Application::get();
    auto &imgui = ImGui::GetIO();
    auto shaders_root = app.asset_man.asset_root_path(AssetType::Shader);
    auto fonts_root = app.asset_man.asset_root_path(AssetType::Font);
    auto roboto_path = (fonts_root / "Roboto-Regular.ttf").string();
    auto fa_solid_900_path = (fonts_root / "fa-solid-900.ttf").string();

    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.GlyphMinAdvanceX = 16.0f;
    font_config.MergeMode = true;
    font_config.PixelSnapH = true;

    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);
    imgui.Fonts->Build();

    u8 *font_data = nullptr;
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);
    auto font_bytes = ls::span(font_data, font_width * font_height * 4);

    // clang-format off
    auto &transfer_man = device->transfer_man();
    self.font_atlas_image = Image::create(
        *device,
        vuk::Format::eR8G8B8A8Unorm,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageType::e2D,
        vuk::Extent3D(font_width, font_height, 1u))
        .value();
    self.font_atlas_view = ImageView::create(
        *device,
        self.font_atlas_image,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    transfer_man.upload_staging(self.font_atlas_view, font_bytes);
    IM_FREE(font_data);
    self.pipeline = Pipeline::create(*device, {
        .module_name = "imgui",
        .root_path = shaders_root,
        .shader_path = shaders_root / "imgui.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    // clang-format on
}

auto ImGuiRenderer::add_image(this ImGuiRenderer &self, vuk::Value<vuk::ImageAttachment> &&attachment) -> ImTextureID {
    ZoneScoped;

    self.rendering_attachments.emplace_back(std::move(attachment));
    return self.rendering_attachments.size();
}

auto ImGuiRenderer::add_image(this ImGuiRenderer &self, ImageView &image_view) -> ImTextureID {
    ZoneScoped;

    auto attachment_info = image_view.get_attachment(*self.device, vuk::ImageUsageFlagBits::eSampled);
    auto attachment = vuk::acquire_ia("imgui_rendering_image_attachment", attachment_info, vuk::Access::eFragmentSampled);
    return self.add_image(std::move(attachment));
}

auto ImGuiRenderer::begin_frame(this ImGuiRenderer &self, f64 delta_time, const vuk::Extent3D &extent) -> void {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    imgui.DeltaTime = static_cast<f32>(delta_time);
    imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

    self.rendering_attachments.clear();
    ImGui::NewFrame();
}

auto ImGuiRenderer::end_frame(this ImGuiRenderer &self, vuk::Value<vuk::ImageAttachment> &&attachment) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    ImGui::Render();

    auto &transfer_man = self.device->transfer_man();
    ImDrawData *draw_data = ImGui::GetDrawData();
    u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (!draw_data || vertex_size_bytes == 0) {
        return std::move(attachment);
    }

    auto vertex_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, vertex_size_bytes);
    auto index_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUtoCPU, index_size_bytes);
    auto vertex_data = vertex_buffer.host_ptr<ImDrawVert>();
    auto index_data = index_buffer.host_ptr<ImDrawIdx>();
    for (const auto *draw_list : draw_data->CmdLists) {
        memcpy(vertex_data, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
        memcpy(index_data, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
        index_data += draw_list->IdxBuffer.Size;
        vertex_data += draw_list->VtxBuffer.Size;
    }

    auto imgui_pass = vuk::make_pass(
        "imgui",
        [draw_data,
         font_atlas_view = self.device->image_view(self.font_atlas_view.id()),
         vertices = vertex_buffer.buffer,
         indices = index_buffer.buffer,
         &pipeline = *self.device->pipeline(self.pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_ARG(vuk::ImageAttachment[], vuk::Access::eFragmentSampled) rendering_images) {
            struct PushConstants {
                glm::vec2 translate = {};
                glm::vec2 scale = {};
            };

            PushConstants c = {};
            c.scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
            c.translate = { -1.0f - draw_data->DisplayPos.x * c.scale.x, -1.0f - draw_data->DisplayPos.y * c.scale.y };

            cmd_list  //
                .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
                .set_rasterization(vuk::PipelineRasterizationStateCreateInfo{})
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .bind_graphics_pipeline(pipeline)
                .bind_index_buffer(indices, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32)
                .bind_vertex_buffer(
                    0, vertices, 0, vuk::Packed{ vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm })
                .push_constants(vuk::ShaderStageFlagBits::eVertex, 0, c);

            ImVec2 clip_off = draw_data->DisplayPos;
            ImVec2 clip_scale = draw_data->FramebufferScale;
            u32 vertex_offset = 0;
            u32 index_offset = 0;
            for (ImDrawList *draw_list : draw_data->CmdLists) {
                for (i32 cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
                    ImDrawCmd &im_cmd = draw_list->CmdBuffer[cmd_i];
                    ImVec4 clip_rect;
                    clip_rect.x = (im_cmd.ClipRect.x - clip_off.x) * clip_scale.x;
                    clip_rect.y = (im_cmd.ClipRect.y - clip_off.y) * clip_scale.y;
                    clip_rect.z = (im_cmd.ClipRect.z - clip_off.x) * clip_scale.x;
                    clip_rect.w = (im_cmd.ClipRect.w - clip_off.y) * clip_scale.y;

                    auto pass_extent = cmd_list.get_ongoing_render_pass().extent;
                    auto fb_scale = ImVec2(static_cast<f32>(pass_extent.width), static_cast<f32>(pass_extent.height));
                    if (clip_rect.x < fb_scale.x && clip_rect.y < fb_scale.y && clip_rect.z >= 0.0f && clip_rect.w >= 0.0f) {
                        if (clip_rect.x < 0.0f) {
                            clip_rect.x = 0.0f;
                        }
                        if (clip_rect.y < 0.0f) {
                            clip_rect.y = 0.0f;
                        }

                        vuk::Rect2D scissor;
                        scissor.offset.x = (int32_t)(clip_rect.x);
                        scissor.offset.y = (int32_t)(clip_rect.y);
                        scissor.extent.width = (uint32_t)(clip_rect.z - clip_rect.x);
                        scissor.extent.height = (uint32_t)(clip_rect.w - clip_rect.y);
                        cmd_list.set_scissor(0, scissor);

                        cmd_list.bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear });
                        if (im_cmd.TextureId != 0) {
                            auto index = im_cmd.TextureId - 1;
                            cmd_list.bind_image(0, 1, rendering_images[index]);
                        } else {
                            cmd_list.bind_image(0, 1, *font_atlas_view);
                        }

                        cmd_list.draw_indexed(
                            im_cmd.ElemCount, 1, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset), 0);
                    }
                }

                vertex_offset += draw_list->VtxBuffer.Size;
                index_offset += draw_list->IdxBuffer.Size;
            }

            return dst;
        });

    auto &imgui_io = ImGui::GetIO();
    imgui_io.Fonts->TexID = self.add_image(self.font_atlas_view);
    auto imgui_rendering_images_arr = vuk::declare_array("imgui_rendering_images", std::span(self.rendering_attachments));

    return imgui_pass(std::move(attachment), std::move(imgui_rendering_images_arr));
}

}  // namespace lr
