#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/World/RenderContext.hh"
#include "Engine/Graphics/VulkanDevice.hh"

#include <imgui.h>

namespace lr::Tasks {
inline auto imgui_build_font_atlas(AssetManager &asset_man) -> ls::pair<std::vector<u8>, glm::ivec2> {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    auto roboto_path = (asset_man.asset_root_path(AssetType::Font) / "Roboto-Regular.ttf").string();
    auto fa_solid_900_path = (asset_man.asset_root_path(AssetType::Font) / "fa-solid-900.ttf").string();

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

    auto bytes_size = font_width * font_height * 4;
    auto bytes = std::vector<u8>(bytes_size);
    std::memcpy(bytes.data(), font_data, bytes_size);
    IM_FREE(font_data);

    return { std::move(bytes), { font_width, font_height } };
}

struct PushConstants {
    glm::vec2 translate = {};
    glm::vec2 scale = {};
    SampledImage sampled_image = {};
};

inline auto imgui_draw(Device &device, vuk::Value<vuk::ImageAttachment> &&input_attachment, WorldRenderContext &render_context)
    -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto pass = vuk::make_pass("imgui", [&](vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst) {
        auto &transfer_man = device.transfer_man();
        auto &imgui = ImGui::GetIO();

        ImDrawData *draw_data = ImGui::GetDrawData();
        u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (!draw_data || vertex_size_bytes == 0) {
            return dst;
        }

        auto pipeline = device.pipeline(render_context.imgui_pipeline.id());
        auto vertex_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, vertex_size_bytes);
        auto index_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, index_size_bytes);
        auto vertex_data = vertex_buffer.host_ptr<ImDrawVert>();
        auto index_data = index_buffer.host_ptr<ImDrawIdx>();
        for (const auto *draw_list : draw_data->CmdLists) {
            memcpy(vertex_data, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(index_data, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            index_data += draw_list->IdxBuffer.Size;
            vertex_data += draw_list->VtxBuffer.Size;
        }

        cmd_list  //
            .set_dynamic_state(vuk::DynamicStateFlagBits::eViewport | vuk::DynamicStateFlagBits::eScissor)
            .set_rasterization(vuk::PipelineRasterizationStateCreateInfo{})
            .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
            .set_viewport(0, { .x = 0, .y = 0, .width = imgui.DisplaySize.x, .height = imgui.DisplaySize.y })
            .bind_graphics_pipeline(*pipeline)
            .bind_persistent(0, device.bindless_descriptor_set())
            .bind_index_buffer(index_buffer.buffer, sizeof(ImDrawIdx) == 2 ? vuk::IndexType::eUint16 : vuk::IndexType::eUint32)
            .bind_vertex_buffer(
                0,
                vertex_buffer.buffer,
                0,
                vuk::Packed{ vuk::Format::eR32G32Sfloat, vuk::Format::eR32G32Sfloat, vuk::Format::eR8G8B8A8Unorm });

        ImVec2 clip_off = draw_data->DisplayPos;
        ImVec2 clip_scale = draw_data->FramebufferScale;
        u32 vertex_offset = 0;
        u32 index_offset = 0;
        for (ImDrawList *draw_list : draw_data->CmdLists) {
            for (i32 cmd_i = 0; cmd_i < draw_list->CmdBuffer.Size; cmd_i++) {
                ImDrawCmd &im_cmd = draw_list->CmdBuffer[cmd_i];

                ImVec2 clip_min((im_cmd.ClipRect.x - clip_off.x) * clip_scale.x, (im_cmd.ClipRect.y - clip_off.y) * clip_scale.y);
                ImVec2 const clip_max((im_cmd.ClipRect.z - clip_off.x) * clip_scale.x, (im_cmd.ClipRect.w - clip_off.y) * clip_scale.y);
                clip_min.x = std::clamp(clip_min.x, 0.0f, static_cast<f32>(imgui.DisplaySize.x));
                clip_min.y = std::clamp(clip_min.y, 0.0f, static_cast<f32>(imgui.DisplaySize.y));
                if (clip_max.x <= clip_min.x || clip_max.y <= clip_min.y) {
                    continue;
                }

                auto rect = vuk::Rect2D::absolute(
                    { i32(clip_min.x), i32(clip_min.y) }, { u32(clip_max.x - clip_min.x), u32(clip_max.y - clip_min.y) });
                cmd_list.set_scissor(0, rect);

                auto rendering_image = render_context.imgui_font_view.id();
                if (im_cmd.TextureId) {
                    auto user_view_id = (ImageViewID)(iptr)(im_cmd.TextureId);
                    rendering_image = user_view_id;
                }

                PushConstants c;
                c.scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
                c.translate = { -1.0f - draw_data->DisplayPos.x * c.scale.x, -1.0f - draw_data->DisplayPos.y * c.scale.y };
                c.sampled_image = { rendering_image, render_context.linear_sampler.id() };
                cmd_list.push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, c);
                cmd_list.draw_indexed(im_cmd.ElemCount, 1, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset), 0);
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }

        return dst;
    });

    return pass(input_attachment);
}

}  // namespace lr::Tasks
