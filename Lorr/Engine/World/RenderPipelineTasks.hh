#pragma once

#include "Engine/Core/Application.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"
#include "Engine/World/Components.hh"

#include <imgui.h>

namespace lr {
struct SkyLUTTask {
    std::string_view name = "Sky LUT";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
        Preset::ColorReadOnly transmittance_lut = {};
        Preset::ColorReadOnly ms_lut = {};
    } uses = {};

    struct PushConstants {
        u64 world_ptr = 0;
        ImageViewID transmittance_image_id = ImageViewID::Invalid;
        ImageViewID ms_lut = ImageViewID::Invalid;
        SamplerID transmittance_sampler_id = SamplerID::Invalid;
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://atmos.lut_vs");
        auto fs = app.asset_man.shader_at("shader://atmos.lut_fs");
        if (!vs || !fs) {
            LOG_ERROR("Shaders are invalid.", name);
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
        auto &transmittance_lut = tc.task_image_data(uses.transmittance_lut);
        auto &ms_lut = tc.task_image_data(uses.ms_lut);
        auto dst_attachment = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .world_ptr = *static_cast<u64 *>(tc.execution_data),
            .transmittance_image_id = transmittance_lut.image_view_id,
            .ms_lut = ms_lut.image_view_id,
            .transmittance_sampler_id = tc.device.create_cached_sampler(SamplerInfo{
                .min_filter = Filtering::Linear,
                .mag_filter = Filtering::Linear,
                .mip_filter = Filtering::Linear,
                .address_u = TextureAddressMode::ClampToEdge,
                .address_v = TextureAddressMode::ClampToEdge,
                .min_lod = -1000,
                .max_lod = 1000,
            }),
        };
        tc.set_push_constants(c);
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct SkyFinalTask {
    std::string_view name = "Sky Final";

    struct Uses {
        Preset::ColorAttachmentRead attachment = {};
        Preset::ColorReadOnly sky_lut = {};
        Preset::ColorReadOnly transmittance_lut = {};
    } uses = {};

    struct PushConstants {
        u64 world_ptr = 0;
        ImageViewID sky_view_image_id = ImageViewID::Invalid;
        ImageViewID transmittance_image_id = ImageViewID::Invalid;
        SamplerID sampler_id = SamplerID::Invalid;
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://atmos.final_vs");
        auto fs = app.asset_man.shader_at("shader://atmos.final_fs");
        if (!vs || !fs) {
            LOG_ERROR("Shaders are invalid.", name);
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
        auto &sky_lut = tc.task_image_data(uses.sky_lut);
        auto &transmittance_lut = tc.task_image_data(uses.transmittance_lut);
        auto dst_attachment = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .world_ptr = *static_cast<u64 *>(tc.execution_data),
            .sky_view_image_id = sky_lut.image_view_id,
            .transmittance_image_id = transmittance_lut.image_view_id,
            .sampler_id = tc.device.create_cached_sampler(SamplerInfo{
                .min_filter = Filtering::Linear,
                .mag_filter = Filtering::Linear,
                .mip_filter = Filtering::Linear,
                .address_u = TextureAddressMode::Repeat,
                .address_v = TextureAddressMode::ClampToEdge,
                .min_lod = -1000,
                .max_lod = 1000,
            }),
        };
        tc.set_push_constants(c);
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct GeometryTask {
    std::string_view name = "Geometry";

    struct Uses {
        Preset::ColorAttachmentRead color_attachment = {};
        Preset::DepthAttachmentWrite depth_attachment = {};
    } uses = {};

    struct PushConstants {
        u64 world_ptr = 0;
        ModelID model_id = ModelID::Invalid;
        MaterialID material_id = MaterialID::Invalid;
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://model_vs");
        auto fs = app.asset_man.shader_at("shader://model_fs");
        if (!vs || !fs) {
            LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(vs.value());
        pipeline_info.set_shader(fs.value());
        pipeline_info.set_vertex_layout(AssetManager::VERTEX_LAYOUT);
        pipeline_info.set_rasterizer_state({ .cull_mode = CullMode::Back });
        pipeline_info.set_depth_stencil_state({ .enable_depth_test = true, .enable_depth_write = true, .depth_compare_op = CompareOp::LessEqual });
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_blend_attachment_all({
            .blend_enabled = true,
            .src_blend = BlendFactor::SrcAlpha,
            .dst_blend = BlendFactor::InvSrcAlpha,
            .blend_op = BlendOp::Add,
            .src_blend_alpha = BlendFactor::One,
            .dst_blend_alpha = BlendFactor::InvSrcAlpha,
            .blend_op_alpha = BlendOp::Add,
        });
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});

        return true;
    }

    void execute(TaskContext &tc) {
        auto &app = Application::get();
        auto &asset_man = app.asset_man;
        auto &world = app.world;

        auto color_attachment_info = tc.as_color_attachment(uses.color_attachment);
        auto depth_attachment_info = tc.as_depth_attachment(uses.depth_attachment, DepthClearValue(1.0f));

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = color_attachment_info,
            .depth_attachment = depth_attachment_info,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .world_ptr = *static_cast<u64 *>(tc.execution_data),
        };

        auto models = world.ecs.query<Component::RenderableModel>();
        models.each([&](Component::RenderableModel &m) {
            c.model_id = static_cast<ModelID>(m.model_id);
            auto &model = asset_man.model_at(c.model_id);

            tc.cmd_list.set_vertex_buffer(model.vertex_buffer.value());
            tc.cmd_list.set_index_buffer(model.index_buffer.value());

            for (auto &mesh : model.meshes) {
                for (auto &primitive_index : mesh.primitive_indices) {
                    auto &primitive = model.primitives[primitive_index];

                    c.material_id = static_cast<MaterialID>(primitive.material_index);
                    tc.set_push_constants(c);
                    tc.cmd_list.draw_indexed(primitive.index_count, primitive.index_offset, static_cast<i32>(primitive.vertex_offset));
                }
            }
        });

        tc.cmd_list.end_rendering();
    }
};

struct GridTask {
    std::string_view name = "Grid";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
        Preset::DepthAttachmentWrite depth_attachment = {};
    } uses = {};

    struct PushConstants {
        u64 world_ptr = 0;
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://editor.grid_vs");
        auto fs = app.asset_man.shader_at("shader://editor.grid_fs");
        if (!vs || !fs) {
            LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(vs.value());
        pipeline_info.set_shader(fs.value());
        pipeline_info.set_rasterizer_state({ .cull_mode = CullMode::None });
        pipeline_info.set_depth_stencil_state({ .enable_depth_test = true, .enable_depth_write = false, .depth_compare_op = CompareOp::LessEqual });
        pipeline_info.set_dynamic_states(DynamicState::Viewport | DynamicState::Scissor);
        pipeline_info.set_blend_attachment_all({
            .blend_enabled = true,
            .src_blend = BlendFactor::SrcAlpha,
            .dst_blend = BlendFactor::InvSrcAlpha,
            .blend_op = BlendOp::Add,
            .src_blend_alpha = BlendFactor::One,
            .dst_blend_alpha = BlendFactor::InvSrcAlpha,
            .blend_op_alpha = BlendOp::Add,
        });
        pipeline_info.set_viewport({});
        pipeline_info.set_scissors({});

        return true;
    }

    void execute(TaskContext &tc) {
        auto dst_attachment = tc.as_color_attachment(uses.attachment);
        auto depth_attachment = tc.as_depth_attachment(uses.depth_attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
            .depth_attachment = depth_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .world_ptr = *static_cast<u64 *>(tc.execution_data),
        };
        tc.set_push_constants(c);
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct ImGuiTask {
    constexpr static std::string_view name = "ImGui";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    struct PushConstants {
        glm::vec2 translate = {};
        glm::vec2 scale = {};
        ImageViewID image_view_id = {};
        SamplerID sampler_id = {};
    };

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
            LOG_ERROR("Shaders are invalid.", name);
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
                .debug_name = "ImGui Vertex Buffer",
            });
        }

        for (BufferID &index_buffer : self.index_buffers) {
            index_buffer = info.device->create_buffer({
                .usage_flags = BufferUsage::Index | BufferUsage::TransferDst,
                .flags = MemoryFlag::Dedicated,
                .preference = MemoryPreference::Device,
                .data_size = ls::mib_to_bytes(32),
                .debug_name = "ImGui Index Buffer",
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
        PushConstants c = {
            .translate = translate,
            .scale = scale,
            .sampler_id = self.sampler,
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

                Rect2D scissor = {
                    .offset = { i32(clip_min.x), i32(clip_min.y) },
                    .extent = { u32(clip_max.x - clip_min.x), u32(clip_max.y - clip_min.y) },
                };
                tc.cmd_list.set_scissors(0, scissor);

                auto im_image_id = static_cast<ImageViewID>(reinterpret_cast<iptr>(im_cmd.TextureId));
                c.image_view_id = im_image_id;

                tc.set_push_constants(c);
                tc.cmd_list.draw_indexed(im_cmd.ElemCount, im_cmd.IdxOffset + index_offset, i32(im_cmd.VtxOffset + vertex_offset));
            }

            vertex_offset += draw_list->VtxBuffer.Size;
            index_offset += draw_list->IdxBuffer.Size;
        }
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr
