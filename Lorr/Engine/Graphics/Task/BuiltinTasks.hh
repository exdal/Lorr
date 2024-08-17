#pragma once

#include "TaskGraph.hh"

#include "Engine/Core/Application.hh"

#include <imgui.h>

namespace lr::BuiltinTask {
struct FullScreenTask {
    std::string_view name = "FullScreen";

    struct Uses {
        Preset::ColorReadOnly source = {};
        Preset::ColorAttachmentWrite destination = {};
    } uses = {};

    struct PushConstants {
        ImageViewID image_view_id = {};
        SamplerID sampler_id = {};
    } push_constants = {};

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://fullscreen_vs");
        auto fs = app.asset_man.shader_at("shader://fullscreen_fs");
        if (!vs || !fs) {
            LR_LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(vs.value());
        pipeline_info.set_shader(fs.value());
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});

        return true;
    }

    void execute(TaskContext &tc) {
        auto &src_image = tc.task_image_data(uses.source);
        auto dst_attachment = tc.as_color_attachment(uses.destination);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        push_constants.image_view_id = src_image.image_view_id;
        push_constants.sampler_id = tc.device.create_cached_sampler(SamplerInfo{
            .min_filter = Filtering::Linear,
            .mag_filter = Filtering::Linear,
            .mip_filter = Filtering::Linear,
            .address_u = TextureAddressMode::Repeat,
            .address_v = TextureAddressMode::Repeat,
            .min_lod = -1000,
            .max_lod = 1000,
        });
        tc.set_push_constants(push_constants);
        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct ImGuiTask {
    std::string_view name = "ImGui";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
        Preset::ColorReadOnly font = {};
    } uses = {};

    struct PushConstants {
        glm::vec2 translate = {};
        glm::vec2 scale = {};
        ImageViewID image_view_id = {};
        SamplerID sampler_id = {};
    } push_constants = {};

    struct {
        SamplerID sampler = SamplerID::Invalid;
        std::array<BufferID, 3> vertex_buffers = {};
        std::array<BufferID, 3> index_buffers = {};
    } self = {};

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://imgui_vs");
        auto fs = app.asset_man.shader_at("shader://imgui_fs");
        if (!vs || !fs) {
            LR_LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(vs.value());
        pipeline_info.set_shader(fs.value());
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});

        VertexAttribInfo vertex_layout[] = {
            { .format = Format::R32G32_SFLOAT, .location = 0, .offset = offsetof(ImDrawVert, pos) },
            { .format = Format::R32G32_SFLOAT, .location = 1, .offset = offsetof(ImDrawVert, uv) },
            { .format = Format::R8G8B8A8_UNORM, .location = 2, .offset = offsetof(ImDrawVert, col) },
        };
        pipeline_info.set_vertex_layout(vertex_layout);
        pipeline_info.set_blend_attachment_all({
            .blend_enabled = true,
            .src_blend = BlendFactor::One,
            .dst_blend = BlendFactor::InvSrcAlpha,
            .blend_op = BlendOp::Add,
            .src_blend_alpha = BlendFactor::One,
            .dst_blend_alpha = BlendFactor::InvSrcAlpha,
            .blend_op_alpha = BlendOp::Add,
        });

        for (BufferID &vertex_buffer : self.vertex_buffers) {
            vertex_buffer = info.device->create_buffer({
                .usage_flags = BufferUsage::Vertex | BufferUsage::TransferDst,
                .flags = MemoryFlag::Dedicated,
                .preference = MemoryPreference::Device,
                .data_size = ls::mib_to_bytes(32),
            });
        }

        for (BufferID &index_buffer : self.index_buffers) {
            index_buffer = info.device->create_buffer({
                .usage_flags = BufferUsage::Index | BufferUsage::TransferDst,
                .flags = MemoryFlag::Dedicated,
                .preference = MemoryPreference::Device,
                .data_size = ls::mib_to_bytes(32),
            });
        }

        self.sampler = info.device->create_cached_sampler({
            .min_filter = Filtering::Linear,
            .mag_filter = Filtering::Linear,
            .mip_filter = Filtering::Linear,
            .address_u = TextureAddressMode::ClampToEdge,
            .address_v = TextureAddressMode::ClampToEdge,
            .min_lod = -1000,
            .max_lod = 1000,
        });

        return true;
    }

    void execute(TaskContext &tc) {
        auto color_attachment = tc.as_color_attachment(uses.attachment);
        auto &task_font = tc.task_image_data(uses.font);
        auto &staging_buffer = tc.staging_buffer();
        auto &imgui = ImGui::GetIO();

        ImDrawData *draw_data = ImGui::GetDrawData();
        u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
        u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
        if (!draw_data || vertex_size_bytes == 0) {
            return;
        }

        auto vertex_buffer_alloc = staging_buffer.alloc(vertex_size_bytes);
        auto index_buffer_alloc = staging_buffer.alloc(index_size_bytes);
        auto vertex_data = reinterpret_cast<ImDrawVert *>(vertex_buffer_alloc.ptr);
        auto index_data = reinterpret_cast<ImDrawIdx *>(index_buffer_alloc.ptr);
        for (const ImDrawList *draw_list : draw_data->CmdLists) {
            memcpy(vertex_data, draw_list->VtxBuffer.Data, draw_list->VtxBuffer.Size * sizeof(ImDrawVert));
            memcpy(index_data, draw_list->IdxBuffer.Data, draw_list->IdxBuffer.Size * sizeof(ImDrawIdx));
            vertex_data += draw_list->VtxBuffer.Size;
            index_data += draw_list->IdxBuffer.Size;
        }

        BufferID vertex_buffer = self.vertex_buffers[tc.frame_index];
        BufferID index_buffer = self.index_buffers[tc.frame_index];
        staging_buffer.upload(vertex_buffer_alloc, vertex_buffer, tc.copy_cmd_list);
        staging_buffer.upload(index_buffer_alloc, index_buffer, tc.copy_cmd_list);

        tc.cmd_list.begin_rendering({
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment,
        });
        tc.cmd_list.set_vertex_buffer(vertex_buffer);
        tc.cmd_list.set_index_buffer(index_buffer, 0, true);
        tc.cmd_list.set_viewport(0, { .x = 0, .y = 0, .width = imgui.DisplaySize.x, .height = imgui.DisplaySize.y, .depth_min = 0.01, .depth_max = 1.0 });

        glm::vec2 scale = { 2.0f / draw_data->DisplaySize.x, 2.0f / draw_data->DisplaySize.y };
        glm::vec2 translate = { -1.0f - draw_data->DisplayPos.x * scale.x, -1.0f - draw_data->DisplayPos.y * scale.y };
        push_constants.scale = scale;
        push_constants.translate = translate;
        push_constants.sampler_id = self.sampler;

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

                Rect2D scissor = {
                    .offset = { i32(clip_min.x), i32(clip_min.y) },
                    .extent = { u32(clip_max.x - clip_min.x), u32(clip_max.y - clip_min.y) },
                };
                tc.cmd_list.set_scissors(0, scissor);

                if (im_cmd.TextureId) {
                    TaskImageID im_image_id = (TaskImageID)(iptr)(im_cmd.TextureId);
                    auto &im_image = tc.task_image_data(im_image_id);
                    push_constants.image_view_id = im_image.image_view_id;
                } else {
                    push_constants.image_view_id = task_font.image_view_id;
                }

                tc.set_push_constants(push_constants);
                tc.cmd_list.draw_indexed(im_cmd.ElemCount, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset));
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr::BuiltinTask
