#pragma once

#include "Engine/Core/Application.hh"

#include "Engine/Graphics/Task/TaskGraph.hh"

#include "Engine/World/Camera.hh"

namespace lr {
struct Atmosphere {
    glm::vec3 rayleigh_scatter = glm::vec3(5.802, 13.558, 33.1) * 1e-6f;
    f32 rayleigh_density = 8.0 * 1e3f;
    f32 planet_radius = 6360.0 * 1e3f;
    f32 atmos_radius = 6460.0 * 1e3f;
    f32 mie_scatter = 3.996 * 1e-6f;
    f32 mie_absorption = 4.4 * 1e-6f;
    f32 mie_density = 1.2 * 1e3f;
    f32 mie_asymmetry = 0.8;
    f32 ozone_height = 25 * 1e3f;
    f32 ozone_thickness = 15 * 1e3f;
    glm::vec3 ozone_absorption = glm::vec3(0.650, 1.881, 0.085) * 1e-6f;
};
static Atmosphere REMOVE_ME_atmos = {};
static BufferID REMOVE_ME_atmos_buffer_id = {};

struct TransmittanceTask {
    std::string_view name = "Transmittance";

    struct Uses {
        Preset::ComputeWrite storage_image = {};
    } uses = {};

    struct PushConstants {
        glm::vec2 image_size = {};
        ImageViewID image_view_id = ImageViewID::Invalid;
        BufferID atmos_buffer_id = BufferID::Invalid;
    };

    struct {
    } self = {};

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto cs = app.asset_man.shader_at("shader://atmos.transmittance");
        if (!cs) {
            LR_LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(cs.value());

        REMOVE_ME_atmos_buffer_id = info.device->create_buffer(BufferInfo{
            .usage_flags = BufferUsage::TransferDst,
            .flags = MemoryFlag::Dedicated,
            .preference = MemoryPreference::Device,
            .data_size = sizeof(Atmosphere),
            .debug_name = "Atmosphere Buffer",
        });

        auto &transfer_queue = info.device->queue_at(CommandType::Transfer);
        auto cmd_list = transfer_queue.begin_command_list(0);
        BufferID temp_buffer = info.device->create_buffer(BufferInfo{
            .usage_flags = BufferUsage::TransferSrc,
            .flags = MemoryFlag::HostSeqWrite,
            .preference = MemoryPreference::Host,
            .data_size = sizeof(Atmosphere),
        });
        transfer_queue.defer(temp_buffer);

        auto gpu_atmos = info.device->buffer_host_data<Atmosphere>(temp_buffer);
        std::memcpy(gpu_atmos, &REMOVE_ME_atmos, sizeof(Atmosphere));
        BufferCopyRegion copy_region = { .src_offset = 0, .dst_offset = 0, .size = sizeof(Atmosphere) };
        cmd_list.copy_buffer_to_buffer(temp_buffer, REMOVE_ME_atmos_buffer_id, copy_region);
        transfer_queue.end_command_list(cmd_list);
        transfer_queue.submit(0, { .self_wait = true });

        return true;
    }

    void execute(TaskContext &tc) {
        auto &storage_image_data = tc.task_image_data(uses.storage_image);
        auto &storage_image = tc.device.image_at(storage_image_data.image_id);

        PushConstants c = {
            .image_size = { storage_image.extent.width, storage_image.extent.height },
            .image_view_id = storage_image_data.image_view_id,
            .atmos_buffer_id = REMOVE_ME_atmos_buffer_id,
        };
        tc.set_push_constants(c);

        tc.cmd_list.dispatch(storage_image.extent.width / 16 + 1, storage_image.extent.height / 16 + 1, 1);
    }
};

struct SkyLUTTask {
    std::string_view name = "Sky LUT";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
        Preset::ColorReadOnly transmittance_lut = {};
    } uses = {};

    struct PushConstants {
        ImageViewID transmittance_image_id = ImageViewID::Invalid;
        SamplerID transmittance_sampler_id = SamplerID::Invalid;
        BufferID atmos_buffer_id = BufferID::Invalid;
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://atmos.lut_vs");
        auto fs = app.asset_man.shader_at("shader://atmos.lut_fs");
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
        auto &transmittance_lut = tc.task_image_data(uses.transmittance_lut);
        auto dst_attachment = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .transmittance_image_id = transmittance_lut.image_view_id,
            .transmittance_sampler_id = tc.device.create_cached_sampler(SamplerInfo{
                .min_filter = Filtering::Linear,
                .mag_filter = Filtering::Linear,
                .mip_filter = Filtering::Linear,
                .address_u = TextureAddressMode::ClampToEdge,
                .address_v = TextureAddressMode::ClampToEdge,
                .min_lod = -1000,
                .max_lod = 1000,
            }),
            .atmos_buffer_id = REMOVE_ME_atmos_buffer_id,
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
    } uses = {};

    struct PushConstants {
        ImageViewID sky_view_image_id = ImageViewID::Invalid;
        SamplerID sky_view_sampler_id = SamplerID::Invalid;
        glm::mat4 view_proj_mat_inv = {};
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://atmos.final_vs");
        auto fs = app.asset_man.shader_at("shader://atmos.final_fs");
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
        auto &app = Application::get();
        auto &scene = app.scene_at(app.active_scene.value());
        auto camera = scene.active_camera->get_mut<PerspectiveCamera>();

        auto &sky_lut = tc.task_image_data(uses.sky_lut);
        auto dst_attachment = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .sky_view_image_id = sky_lut.image_view_id,
            .sky_view_sampler_id = tc.device.create_cached_sampler(SamplerInfo{
                .min_filter = Filtering::Linear,
                .mag_filter = Filtering::Linear,
                .mip_filter = Filtering::Linear,
                .address_u = TextureAddressMode::Repeat,
                .address_v = TextureAddressMode::ClampToEdge,
                .min_lod = -1000,
                .max_lod = 1000,
            }),
            .view_proj_mat_inv = glm::inverse(glm::transpose(camera->projection_matrix)) * glm::inverse(glm::transpose(camera->view_matrix)),
        };
        tc.set_push_constants(c);
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

struct GridTask {
    std::string_view name = "Grid";

    struct Uses {
        Preset::ColorAttachmentWrite attachment = {};
    } uses = {};

    struct PushConstants {
        glm::mat4 view_proj_mat_inv = {};
    };

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto vs = app.asset_man.shader_at("shader://editor.grid_vs");
        auto fs = app.asset_man.shader_at("shader://editor.grid_fs");
        if (!vs || !fs) {
            LR_LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(vs.value());
        pipeline_info.set_shader(fs.value());
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
        auto &scene = app.scene_at(app.active_scene.value());
        auto camera = scene.active_camera->get_mut<PerspectiveCamera>();

        auto dst_attachment = tc.as_color_attachment(uses.attachment);

        tc.cmd_list.begin_rendering(RenderingBeginInfo{
            .render_area = tc.pass_rect(),
            .color_attachments = dst_attachment,
        });

        tc.cmd_list.set_viewport(0, tc.pass_viewport());
        tc.cmd_list.set_scissors(0, tc.pass_rect());

        PushConstants c = {
            .view_proj_mat_inv = glm::inverse(glm::transpose(camera->projection_matrix)) * glm::inverse(glm::transpose(camera->view_matrix)),
        };
        tc.set_push_constants(c);
        tc.cmd_list.draw(3);
        tc.cmd_list.end_rendering();
    }
};

}  // namespace lr
