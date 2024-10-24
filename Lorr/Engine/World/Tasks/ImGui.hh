#pragma once

#include "Engine/Asset/Asset.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/RenderContext.hh"

#include <imgui.h>

namespace lr::Tasks {
inline GraphicsPipelineInfo imgui_pipeline_info(AssetManager &asset_man, vk::Format attachment_format) {
    auto shaders_root = asset_man.asset_dir(AssetType::Shader);

    return {
        .color_attachment_formats = { attachment_format },
        .shader_module_info = {
            .module_name = "imgui",
            .root_path = shaders_root,
            .shader_path = shaders_root / "imgui.slang",
            .entry_points = { "vs_main", "fs_main" },
        },
        .vertex_attrib_infos = {
            { .format = vk::Format::R32G32_SFLOAT, .location = 0, .offset = offsetof(ImDrawVert, pos) },
            { .format = vk::Format::R32G32_SFLOAT, .location = 1, .offset = offsetof(ImDrawVert, uv) },
            { .format = vk::Format::R8G8B8A8_UNORM, .location = 2, .offset = offsetof(ImDrawVert, col) },
        },
        .depth_stencil_state = {
            .enable_depth_test = true,
            .enable_depth_write = true,
            .depth_compare_op = vk::CompareOp::LessEqual,
        },
        .blend_attachments = { {
            .blend_enabled = true,
            .src_blend = vk::BlendFactor::SrcAlpha,
            .dst_blend = vk::BlendFactor::InvSrcAlpha,
            .blend_op = vk::BlendOp::Add,
            .src_blend_alpha = vk::BlendFactor::One,
            .dst_blend_alpha = vk::BlendFactor::InvSrcAlpha,
            .blend_op_alpha = vk::BlendOp::Add,
        } },
    };
}

inline std::pair<u8 *, glm::ivec2> imgui_build_font_atlas(AssetManager &asset_man) {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    auto roboto_path = (asset_man.asset_dir(AssetType::Font) / "Roboto-Regular.ttf").string();
    auto fa_solid_900_path = (asset_man.asset_dir(AssetType::Font) / "fa-solid-900.ttf").string();

    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.GlyphMinAdvanceX = 16.0f;
    font_config.MergeMode = true;
    font_config.PixelSnapH = true;

    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);

    font_config.GlyphMinAdvanceX = 32.0f;
    font_config.MergeMode = false;
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 32.0f, &font_config, icons_ranges);

    imgui.Fonts->Build();

    u8 *font_data = nullptr;  // imgui context frees this itself
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);

    return { font_data, { font_width, font_height } };
}

struct ImGuiTask {
    constexpr static std::string_view name = "ImGui";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    TaskImageID font_atlas_image = {};
    PipelineID pipeline = {};

    void execute(TaskContext &tc) {
        struct PushConstants {
            glm::vec2 translate = translate;
            glm::vec2 scale = scale;
            ImageViewID image_view_id = {};
            SamplerID sampler_id = {};
        };

        auto color_attachment = tc.as_color_attachment(uses.attachment, vk::ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f));
        auto &render_context = tc.exec_data_as<WorldRenderContext>();
        auto &imgui = ImGui::GetIO();

        ImDrawData *draw_data = ImGui::GetDrawData();
        u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (!draw_data || vertex_size_bytes == 0) {
            return;
        }

        auto vertex_buffer_alloc = tc.transient_buffer(vertex_size_bytes);
        auto vertex_data = reinterpret_cast<ImDrawVert *>(vertex_buffer_alloc.ptr);
        auto index_buffer_alloc = tc.transient_buffer(index_size_bytes);
        auto index_data = reinterpret_cast<ImDrawIdx *>(index_buffer_alloc.ptr);
        for (const ImDrawList *draw_list : draw_data->CmdLists) {
            memcpy(vertex_data, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(index_data, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            index_data += draw_list->IdxBuffer.Size;
            vertex_data += draw_list->VtxBuffer.Size;
        }

        tc.set_pipeline(this->pipeline);

        RenderingBeginInfo rendering_info = {
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment,
        };
        tc.cmd_list.begin_rendering(rendering_info);
        tc.cmd_list.set_vertex_buffer(vertex_buffer_alloc.buffer_id, vertex_buffer_alloc.offset);
        tc.cmd_list.set_index_buffer(index_buffer_alloc.buffer_id, index_buffer_alloc.offset, true);
        tc.cmd_list.set_viewport(
            { .x = 0, .y = 0, .width = imgui.DisplaySize.x, .height = imgui.DisplaySize.y, .depth_min = 0.01, .depth_max = 1.0 });

        glm::vec2 scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
        glm::vec2 translate = { -1.0f - draw_data->DisplayPos.x * scale.x, -1.0f - draw_data->DisplayPos.y * scale.y };
        PushConstants c = {
            .translate = translate,
            .scale = scale,
            .image_view_id = tc.image_view(this->font_atlas_image),
            .sampler_id = render_context.linear_sampler,
        };
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

                vk::Rect2D scissor = {
                    .offset = { i32(clip_min.x), i32(clip_min.y) },
                    .extent = { u32(clip_max.x - clip_min.x), u32(clip_max.y - clip_min.y) },
                };
                tc.cmd_list.set_scissors(scissor);

                if (im_cmd.TextureId) {
                    auto im_image_id = (TaskImageID)(iptr)(im_cmd.TextureId);
                    c.image_view_id = tc.image_view(im_image_id);
                }

                tc.set_push_constants(c);
                tc.cmd_list.draw_indexed(im_cmd.ElemCount, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset));
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }

        tc.cmd_list.end_rendering();
    }
};
}  // namespace lr::Tasks
