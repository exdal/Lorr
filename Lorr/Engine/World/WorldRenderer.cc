#include "Engine/World/WorldRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/World/Components.hh"

#include "Engine/World/Tasks/Cloud.hh"
#include "Engine/World/Tasks/Geometry.hh"
#include "Engine/World/Tasks/Grid.hh"
#include "Engine/World/Tasks/ImGui.hh"
#include "Engine/World/Tasks/PostProcess.hh"
#include "Engine/World/Tasks/Sky.hh"

#include <glm/gtx/component_wise.hpp>

namespace lr {
struct FrameResources {
    Image composition_result = {};
    Image final_image = {};
    Image vis_depth_image = {};

    FrameResources(Device &device, SwapChain &swap_chain) {
        // clang-format off
        composition_result = Image::create(
            device,
            vk::ImageUsage::ColorAttachment | vk::ImageUsage::Sampled,
            swap_chain.format(),
            vk::ImageType::View2D,
            swap_chain.extent())
            .value()
            .set_name("Editor View Image")
            .transition(vk::ImageLayout::Undefined, vk::ImageLayout::ColorAttachment);

        final_image = Image::create(
            device,
            vk::ImageUsage::ColorAttachment | vk::ImageUsage::Sampled,
            vk::Format::R32G32B32A32_SFLOAT,
            vk::ImageType::View2D,
            swap_chain.extent())
            .value()
            .set_name("Final Image")
            .transition(vk::ImageLayout::Undefined, vk::ImageLayout::ColorAttachment);

        vis_depth_image = Image::create(
            device,
            vk::ImageUsage::DepthStencilAttachment | vk::ImageUsage::Sampled,
            vk::Format::D32_SFLOAT,
            vk::ImageType::View2D,
            swap_chain.extent(),
            vk::ImageAspectFlag::Depth)
            .value()
            .set_name("Geo Depth Image")
            .transition(vk::ImageLayout::Undefined, vk::ImageLayout::DepthAttachment);

        // clang-format on
    }

    ~FrameResources() {
        composition_result.destroy();
        final_image.destroy();
        vis_depth_image.destroy();
    }
};

template<>
struct Handle<WorldRenderer>::Impl {
    Device device = {};
    WorldRenderer::PBRFlags pbr_flags = {};
    TaskGraph pbr_graph = {};
    TransferManager transfer_man = {};
    WorldRenderContext context = {};

    Sampler linear_sampler = {};
    Sampler nearest_sampler = {};

    TaskImageID swap_chain_image = TaskImageID::Invalid;

    // PERSISTENT IMAGES
    // Images that DO NOT rely on Swap Chain dimensions
    Image imgui_font_image = {};
    Image sky_transmittance_image = {};
    Image sky_multiscatter_image = {};
    Image sky_aerial_perspective_image = {};
    Image sky_view_image = {};
    Image cloud_shape_noise_image = {};
    Image cloud_detail_noise_image = {};

    // FRAME IMAGES
    // Images that DO rely on Swap Chain dimensions
    std::unique_ptr<FrameResources> frame_resources = {};
};

auto WorldRenderer::create(Device device) -> WorldRenderer {
    auto impl = new Impl;
    auto self = WorldRenderer(impl);
    impl->device = device;
    impl->transfer_man = TransferManager::create_with_gpu(impl->device, ls::mib_to_bytes(8), vk::BufferUsage::Storage);

    self.setup_persistent_resources();

    // TODO: Remove later
    impl->context.sun_angle = { 90, 45 };
    self.update_sun_dir();

    return self;
}

auto WorldRenderer::setup_persistent_resources() -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    // clang-format off
    impl->linear_sampler = Sampler::create(
        impl->device,
        vk::Filtering::Linear,
        vk::Filtering::Linear,
        vk::Filtering::Linear,
        vk::SamplerAddressMode::ClampToEdge,
        vk::SamplerAddressMode::ClampToEdge,
        vk::SamplerAddressMode::ClampToEdge,
        vk::CompareOp::Never)
        .value();
    impl->nearest_sampler = Sampler::create(
        impl->device,
        vk::Filtering::Nearest,
        vk::Filtering::Nearest,
        vk::Filtering::Nearest,
        vk::SamplerAddressMode::Repeat,
        vk::SamplerAddressMode::Repeat,
        vk::SamplerAddressMode::Repeat,
        vk::CompareOp::Never)
        .value();

    auto [imgui_atlas_data, imgui_atlas_dimensions] = Tasks::imgui_build_font_atlas(asset_man);
    impl->imgui_font_image = Image::create(
        impl->device,
        vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
        vk::Format::R8G8B8A8_UNORM,
        vk::ImageType::View2D,
        vk::Extent3D(imgui_atlas_dimensions.x, imgui_atlas_dimensions.y, 1u))
        .value()
        .set_name("ImGui Font Atlas")
        .set_data(imgui_atlas_data, glm::compMul(imgui_atlas_dimensions) * 4, vk::ImageLayout::ShaderReadOnly);

    impl->sky_transmittance_image = Image::create(
        impl->device,
        vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
        vk::Format::R16G16B16A16_SFLOAT,
        vk::ImageType::View2D,
        { 256, 64, 1 })
        .value()
        .set_name("Sky Transmittance Image")
        .transition(vk::ImageLayout::Undefined, vk::ImageLayout::General);

    impl->sky_multiscatter_image = Image::create(
        impl->device,
        vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
        vk::Format::R16G16B16A16_SFLOAT,
        vk::ImageType::View2D,
        { 32, 32, 1 })  // dont change
        .value()
        .set_name("Sky Multiscatter Image")
        .transition(vk::ImageLayout::Undefined, vk::ImageLayout::General);

    impl->sky_aerial_perspective_image = Image::create(
        impl->device,
        vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
        vk::Format::R16G16B16A16_SFLOAT,
        vk::ImageType::View3D,
        { 32, 32, 32 })
        .value()
        .set_name("Sky Aerial Perspective Image")
        .transition(vk::ImageLayout::Undefined, vk::ImageLayout::General);

    impl->sky_view_image = Image::create(
        impl->device,
        vk::ImageUsage::ColorAttachment | vk::ImageUsage::Sampled,
        vk::Format::R16G16B16A16_SFLOAT,
        vk::ImageType::View2D,
        { 200, 100, 1 })
        .value()
        .set_name("Sky View Image")
        .transition(vk::ImageLayout::Undefined, vk::ImageLayout::ColorAttachment);

    impl->cloud_shape_noise_image = Image::create(
        impl->device,
        vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
        vk::Format::R8G8B8A8_UNORM,
        vk::ImageType::View3D,
        { 256, 256, 256 })
        .value()
        .set_name("Cloud Shape Noise Image")
        .transition(vk::ImageLayout::Undefined, vk::ImageLayout::General);

    impl->cloud_detail_noise_image = Image::create(
        impl->device,
        vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
        vk::Format::R8G8B8A8_UNORM,
        vk::ImageType::View3D,
        { 128, 128, 128 })
        .value()
        .set_name("Cloud Detail Noise Image")
        .transition(vk::ImageLayout::Undefined, vk::ImageLayout::General);
    // clang-format on

    this->update_world_data();
    this->record_setup_graph();
}

auto WorldRenderer::record_setup_graph() -> void {
    ZoneScoped;

    LS_EXPECT(impl->context.world_ptr != 0);

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto task_graph = TaskGraph::create({ .device = impl->device });

    impl->device.wait();
    auto sky_transmittance_pipeline = Pipeline::create(impl->device, Tasks::sky_transmittance_pipeline_info(asset_man)).value();
    auto sky_transmittance_task_image = task_graph.add_image({
        .image_id = impl->sky_transmittance_image.id(),
    });

    task_graph.add_task(InlineTask{
        .uses = {
            Preset::ComputeWrite(sky_transmittance_task_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &render_context = tc.exec_data_as<WorldRenderContext>();
            auto &lut_use = tc.task_use(0);
            auto &lut_info = tc.task_image_data(lut_use);
            auto lut_image = tc.device.image(lut_info.image_id);

            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec2 image_size = {};
                ImageViewID image_view_id = ImageViewID::Invalid;
            };

            tc.set_pipeline(sky_transmittance_pipeline.id());
            tc.set_push_constants(PushConstants{
                .world_ptr = render_context.world_ptr,
                .image_size = { lut_image.extent().width, lut_image.extent().height },
                .image_view_id = lut_image.view().id(),
            });
            tc.cmd_list.dispatch(lut_image.extent().width / 16 + 1, lut_image.extent().height / 16 + 1, 1);
        }
    });

    auto sky_multiscatter_pipeline = Pipeline::create(impl->device, Tasks::sky_multiscatter_pipeline_info(asset_man)).value();
    auto sky_multiscatter_task_image = task_graph.add_image({
        .image_id = impl->sky_multiscatter_image.id(),
    });

    task_graph.add_task(InlineTask{
        .uses = {
            Preset::ComputeWrite(sky_multiscatter_task_image),
            Preset::ComputeRead(sky_transmittance_task_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            struct PushConstants {
                u64 world_ptr = 0;
                ImageViewID transmittance_image_id = {};
                glm::vec2 ms_image_size = {};
                ImageViewID ms_image_id = ImageViewID::Invalid;
            };

            auto &render_context = tc.exec_data_as<WorldRenderContext>();
            auto &ms_lut_use = tc.task_use(0);
            auto &ms_lut_info = tc.task_image_data(ms_lut_use);
            auto ms_lut_image = tc.device.image(ms_lut_info.image_id);

            auto &transmittance_lut_use = tc.task_use(1);
            auto &transmittance_lut_info = tc.task_image_data(transmittance_lut_use);
            auto transmittance_lut_image = tc.device.image(transmittance_lut_info.image_id);

            tc.set_pipeline(sky_multiscatter_pipeline.id());
            tc.set_push_constants(PushConstants{
                .world_ptr = render_context.world_ptr,
                .transmittance_image_id = transmittance_lut_image.view().id(),
                .ms_image_size = { ms_lut_image.extent().width, ms_lut_image.extent().height },
                .ms_image_id = ms_lut_image.view().id(),
            });
            tc.cmd_list.dispatch(ms_lut_image.extent().width / 16 + 1, ms_lut_image.extent().height / 16 + 1, 1);

            tc.cmd_list.image_transition({
                .src_stage = transmittance_lut_use.access.stage,
                .src_access = transmittance_lut_use.access.access,
                .dst_stage = vk::PipelineStage::FragmentShader,
                .dst_access = vk::MemoryAccess::ShaderRead,
                .old_layout = transmittance_lut_use.image_layout,
                .new_layout = vk::ImageLayout::ShaderReadOnly,
                .image_id = transmittance_lut_info.image_id,
            });
            tc.cmd_list.image_transition({
                .src_stage = ms_lut_use.access.stage,
                .src_access = ms_lut_use.access.access,
                .dst_stage = vk::PipelineStage::FragmentShader,
                .dst_access = vk::MemoryAccess::ShaderRead,
                .old_layout = ms_lut_use.image_layout,
                .new_layout = vk::ImageLayout::ShaderReadOnly,
                .image_id = ms_lut_info.image_id,
            });
        }
    });

    auto cloud_shape_noise_pipeline = Pipeline::create(impl->device, Tasks::cloud_shape_noise_pipeline_info(asset_man)).value();
    auto cloud_shape_noise_task_image = task_graph.add_image({
        .image_id = impl->cloud_shape_noise_image.id(),
    });

    task_graph.add_task(InlineTask{
        .uses = {
            Preset::ComputeWrite(cloud_shape_noise_task_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &cloud_noise_image_use = tc.task_use(0);
            auto cloud_noise_task_image_info = tc.task_image_data(cloud_noise_image_use);
            auto cloud_noise_image_info = tc.device.image(cloud_noise_task_image_info.image_id);
            auto image_extent = cloud_noise_image_info.extent();

            struct PushConstants {
                glm::vec3 image_size = {};
                ImageViewID detail_image_view = ImageViewID::Invalid;
            };

            tc.set_pipeline(cloud_shape_noise_pipeline.id());
            tc.set_push_constants(PushConstants{
                .image_size = glm::vec3(1.0) / glm::vec3(f32(image_extent.width), f32(image_extent.height), image_extent.depth),
                .detail_image_view = cloud_noise_image_info.view().id(),
            });
            tc.cmd_list.dispatch(image_extent.width / 16, image_extent.height / 16, image_extent.depth);

            tc.cmd_list.image_transition({
                .src_stage = cloud_noise_image_use.access.stage,
                .src_access = cloud_noise_image_use.access.access,
                .dst_stage = vk::PipelineStage::FragmentShader,
                .dst_access = vk::MemoryAccess::ShaderRead,
                .old_layout = cloud_noise_image_use.image_layout,
                .new_layout = vk::ImageLayout::ShaderReadOnly,
                .image_id = cloud_noise_task_image_info.image_id,
            });
        }
    });

    auto cloud_detail_noise_pipeline = Pipeline::create(impl->device, Tasks::cloud_detail_noise_pipeline_info(asset_man)).value();
    auto cloud_detail_noise_task_image = task_graph.add_image({
        .image_id = impl->cloud_detail_noise_image.id(),
    });

    task_graph.add_task(InlineTask{
        .uses = {
            Preset::ComputeWrite(cloud_detail_noise_task_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &cloud_noise_image_use = tc.task_use(0);
            auto cloud_noise_task_image_info = tc.task_image_data(cloud_noise_image_use);
            auto cloud_noise_image_info = tc.device.image(cloud_noise_task_image_info.image_id);
            auto image_extent = cloud_noise_image_info.extent();

            struct PushConstants {
                glm::vec3 image_size = {};
                ImageViewID detail_image_view = ImageViewID::Invalid;
            };

            tc.set_pipeline(cloud_detail_noise_pipeline.id());
            tc.set_push_constants(PushConstants{
                .image_size = glm::vec3(1.0) / glm::vec3(f32(image_extent.width), f32(image_extent.height), image_extent.depth),
                .detail_image_view = cloud_noise_image_info.view().id(),
            });
            tc.cmd_list.dispatch(image_extent.width / 16, image_extent.height / 16, image_extent.depth);

            tc.cmd_list.image_transition({
                .src_stage = cloud_noise_image_use.access.stage,
                .src_access = cloud_noise_image_use.access.access,
                .dst_stage = vk::PipelineStage::FragmentShader,
                .dst_access = vk::MemoryAccess::ShaderRead,
                .old_layout = cloud_noise_image_use.image_layout,
                .new_layout = vk::ImageLayout::ShaderReadOnly,
                .image_id = cloud_noise_task_image_info.image_id,
            });
        }
    });

    auto transfer_sema = impl->device.queue(vk::CommandType::Transfer).semaphore();
    task_graph.execute({
        .frame_index = 0,
        .execution_data = &impl->context,
        .wait_semas = transfer_sema,
        .signal_semas = transfer_sema,
    });
    impl->device.wait();
    task_graph.destroy();
}

auto WorldRenderer::record_pbr_graph(SwapChain &swap_chain) -> void {
    ZoneScoped;

    auto &asset_man = Application::get().asset_man;

    if (impl->pbr_graph) {
        impl->pbr_graph.destroy();
    }

    impl->frame_resources.reset();

    impl->pbr_graph = TaskGraph::create({
        .device = impl->device,
        .staging_buffer_size = ls::mib_to_bytes(16),
        .staging_buffer_uses = vk::BufferUsage::Vertex | vk::BufferUsage::Index,
    });
    impl->swap_chain_image = impl->pbr_graph.add_image({});
    impl->frame_resources = std::make_unique<FrameResources>(impl->device, swap_chain);

    // ── SKY VIEW PASS ───────────────────────────────────────────────────
    auto sky_transmittance_task_image = impl->pbr_graph.add_image({
        .image_id = impl->sky_transmittance_image.id(),
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    auto sky_multiscatter_task_image = impl->pbr_graph.add_image({
        .image_id = impl->sky_multiscatter_image.id(),
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    auto sky_view_task_image = impl->pbr_graph.add_image({
        .image_id = impl->sky_view_image.id(),
        .layout = vk::ImageLayout::ColorAttachment,
        .access = vk::PipelineAccess::ColorAttachmentWrite,
    });

    if (impl->pbr_flags.render_sky) {
        LS_EXPECT(impl->context.world_data.cameras_ptr != 0);
        impl->pbr_graph.add_task(Tasks::SkyViewTask{
            .uses = {
                .sky_view_attachment = sky_view_task_image,
                .transmittance_lut = sky_transmittance_task_image,
                .ms_lut = sky_multiscatter_task_image,
            },
            .pipeline = Pipeline::create(impl->device, Tasks::sky_view_pipeline_info(asset_man,
                        impl->sky_view_image.format())).value(),
        });
    }

    // ── SKY AERIAL PASS ─────────────────────────────────────────────────
    auto sky_aerial_perspective_task_image = impl->pbr_graph.add_image({
        .image_id = impl->sky_aerial_perspective_image.id(),
        .layout = vk::ImageLayout::General,
        .access = vk::PipelineAccess::ComputeWrite,
    });

    if (impl->pbr_flags.render_sky && impl->pbr_flags.render_aerial_perspective) {
        impl->pbr_graph.add_task(Tasks::SkyAerialTask{
            .uses = {
                .aerial_lut = sky_aerial_perspective_task_image,
                .transmittance_lut = sky_transmittance_task_image,
                .multiscatter_lut = sky_multiscatter_task_image,
            },
            .pipeline = Pipeline::create(impl->device, Tasks::sky_aerial_pipeline_info(asset_man)).value(),
        });
    }

    // ── SKY FINAL TASK ──────────────────────────────────────────────────
    auto final_task_image = impl->pbr_graph.add_image({
        .image_id = impl->frame_resources->final_image.id(),
        .layout = vk::ImageLayout::ColorAttachment,
        .access = vk::PipelineAccess::ColorAttachmentWrite,
    });

    if (impl->pbr_flags.render_sky) {
        impl->pbr_graph.add_task(Tasks::SkyFinalTask{
            .uses = {
                .color_attachment = final_task_image,
                .sky_view_lut = sky_view_task_image,
                .transmittance_lut = sky_transmittance_task_image,
            },
            .pipeline = Pipeline::create(impl->device, Tasks::sky_final_pipeline_info(asset_man,
                        impl->frame_resources->final_image.format())).value(),
            .sky_sampler = Sampler::create(
                impl->device,
                vk::Filtering::Linear,
                vk::Filtering::Linear,
                vk::Filtering::Linear,
                vk::SamplerAddressMode::ClampToEdge,
                vk::SamplerAddressMode::ClampToEdge,
                vk::SamplerAddressMode::ClampToEdge,
                vk::CompareOp::Never).value(),
        });
    }

    // ────────────────────────────────────────────────────────────────────
    // VIS BEGIN
    // ── GEOMETRY ────────────────────────────────────────────────────────
    auto vis_depth_task_image = impl->pbr_graph.add_image({
        .image_id = impl->frame_resources->vis_depth_image.id(),
        .layout = vk::ImageLayout::DepthAttachment,
        .access = vk::PipelineAccess::DepthStencilWrite,
    });

    impl->pbr_graph.add_task(Tasks::GeometryTask{
        .uses = {
            .color_attachment = final_task_image,
            .depth_attachment = vis_depth_task_image,
            .transmittance_image = sky_transmittance_task_image,
        },
        .pipeline = Pipeline::create(impl->device, Tasks::geometry_pipeline_info(asset_man,
                    impl->frame_resources->final_image.format(), impl->frame_resources->vis_depth_image.format())).value(),
    });

    // ── GRID TASK ───────────────────────────────────────────────────────
    if (impl->pbr_flags.render_editor_grid) {
        impl->pbr_graph.add_task(Tasks::GridTask{
            .uses = {
                .color_attachment = final_task_image,
                .depth_attachment = vis_depth_task_image,
            },
            .pipeline = Pipeline::create(impl->device, Tasks::grid_pipeline_info(asset_man,
                        impl->frame_resources->final_image.format(), impl->frame_resources->vis_depth_image.format())).value(),
        });
    }

    // ── CLOUD PASS ──────────────────────────────────────────────────────
    auto cloud_shape_noise_task_image = impl->pbr_graph.add_image({
        .image_id = impl->cloud_shape_noise_image.id(),
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    auto cloud_detail_noise_task_image = impl->pbr_graph.add_image({
        .image_id = impl->cloud_detail_noise_image.id(),
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    if (impl->pbr_flags.render_clouds) {
        LS_EXPECT(impl->context.world_data.cameras_ptr != 0);
        impl->pbr_graph.add_task(Tasks::CloudDraw{
            .uses = {
                .color_attachment = final_task_image,
                .transmittance_lut_image = sky_transmittance_task_image,
                .aerial_perspective_lut_image = sky_aerial_perspective_task_image,
                .cloud_shape_image = cloud_shape_noise_task_image,
                .cloud_detail_image = cloud_detail_noise_task_image,
            },
            .pipeline = Pipeline::create(impl->device, Tasks::draw_clouds_pipeline_info(asset_man,
                        impl->frame_resources->final_image.format())).value(),
            .sampler = Sampler::create(
                impl->device,
                vk::Filtering::Linear,
                vk::Filtering::Linear,
                vk::Filtering::Linear,
                vk::SamplerAddressMode::Repeat,
                vk::SamplerAddressMode::Repeat,
                vk::SamplerAddressMode::Repeat,
                vk::CompareOp::Never).value(),
        });
    }

    // ── SKY APPLY AERIAL PASS ───────────────────────────────────────────
    if (impl->pbr_flags.render_sky && impl->pbr_flags.render_aerial_perspective) {
        LS_EXPECT(impl->context.world_data.cameras_ptr != 0);
        impl->pbr_graph.add_task(Tasks::SkyApplyAerialTask{
            .uses = {
                .color_attachment = final_task_image,
                .depth_image = vis_depth_task_image,
                .aerial_perspective_image = sky_aerial_perspective_task_image,
            },
            .pipeline = Pipeline::create(impl->device, Tasks::sky_apply_aerial_pipeline_info(asset_man,
                        impl->frame_resources->final_image.format())).value(),
        });
    }

    // ── POST PROCESSING ─────────────────────────────────────────────────
    auto composition_result_task_image = impl->pbr_graph.add_image({
        .image_id = impl->frame_resources->composition_result.id(),
        .layout = vk::ImageLayout::ColorAttachment,
        .access = vk::PipelineAccess::ColorAttachmentWrite,
    });

    impl->pbr_graph.add_task(Tasks::TonemapTask{
        .uses = {
            .dst_attachment = composition_result_task_image,
            .src_image = final_task_image,
        },
        .pipeline = Pipeline::create(impl->device, Tasks::tonemap_pipeline_info(asset_man,
                    impl->frame_resources->composition_result.format())).value()
    });

    if (impl->pbr_flags.render_imgui) {
        impl->pbr_graph.add_task(Tasks::ImGuiTask{
            .uses = {
                .attachment = impl->swap_chain_image,
            },
            .font_atlas_image = impl->imgui_font_image.id(),
            .pipeline = Pipeline::create(impl->device, Tasks::imgui_pipeline_info(asset_man,
                        swap_chain.format())).value()
        });
    }

    // TODO: RUNTIME: Add fullscreen tri pass here for composition result when we start runtime

    impl->pbr_graph.present(impl->swap_chain_image);
}

auto WorldRenderer::get_pbr_flags() -> WorldRenderer::PBRFlags & {
    return impl->pbr_flags;
}

auto WorldRenderer::update_pbr_graph() -> void {
    ZoneScoped;

    impl->device.wait();
    impl->pbr_graph.destroy();
}

auto WorldRenderer::begin_scene_render_data(u32 camera_count, u32 model_transform_count) -> ls::option<WorldRenderer::SceneBeginInfo> {
    ZoneScoped;

    auto gpu_cameras_alloc = impl->transfer_man.allocate(sizeof(GPUCameraData) * camera_count);
    if (!gpu_cameras_alloc.has_value()) {
        LOG_ERROR("Failed to allocate GPU Camera Buffer!");
        return ls::nullopt;
    }

    auto gpu_model_transforms_alloc = impl->transfer_man.allocate(sizeof(GPUModelTransformData) * model_transform_count);
    if (!gpu_model_transforms_alloc.has_value()) {
        LOG_ERROR("Failed to allocate GPU Model Transform Buffer!");

        impl->transfer_man.discard(gpu_cameras_alloc.value());

        return ls::nullopt;
    }

    return WorldRenderer::SceneBeginInfo{
        .cameras_allocation = gpu_cameras_alloc.value(),
        .model_transfors_allocation = gpu_model_transforms_alloc.value(),
    };
}

auto WorldRenderer::end_scene_render_data(WorldRenderer::SceneBeginInfo &info) -> void {
    ZoneScoped;

    ls::static_vector<GPUAllocation, 2> allocations = {};
    auto &world_data = impl->context.world_data;
    world_data.active_camera_index = info.active_camera;
    world_data.cameras_ptr = 0;
    world_data.model_transforms_ptr = 0;
    world_data.materials_ptr =
        info.materials_buffer_id != BufferID::Invalid ? impl->device.buffer(info.materials_buffer_id).device_address() : 0;
    impl->context.models = info.gpu_models;

    if (info.cameras_allocation.size != 0) {
        world_data.cameras_ptr = info.cameras_allocation.gpu_offset();
        allocations.push_back(info.cameras_allocation);
    }
    if (info.model_transfors_allocation.size != 0) {
        world_data.model_transforms_ptr = info.model_transfors_allocation.gpu_offset();
        allocations.push_back(info.model_transfors_allocation);
    }

    impl->transfer_man.upload(allocations);
}

auto WorldRenderer::update_world_data() -> void {
    ZoneScoped;

    auto &context = impl->context;
    auto &world_data = context.world_data;

    world_data.linear_sampler = impl->linear_sampler.id();
    world_data.nearest_sampler = impl->nearest_sampler.id();

    auto gpu_world_alloc = impl->transfer_man.allocate(sizeof(GPUWorldData)).value();
    std::memcpy(gpu_world_alloc.ptr, &world_data, sizeof(GPUWorldData));
    impl->context.world_ptr = gpu_world_alloc.gpu_offset();
    impl->transfer_man.upload(gpu_world_alloc);
}

auto WorldRenderer::render(SwapChain &swap_chain, Image &image) -> bool {
    ZoneScoped;

    auto transfer_sema = impl->transfer_man.semaphore();
    impl->transfer_man.collect_garbage();
    this->update_world_data();

    if (!impl->pbr_graph) {
        impl->device.wait();
        this->record_pbr_graph(swap_chain);
    }

    impl->pbr_graph.set_image(impl->swap_chain_image, { .image_id = image.id() });
    impl->pbr_graph.execute(TaskExecuteInfo{
        .frame_index = swap_chain.image_index(),
        .execution_data = &impl->context,
        .wait_semas = {},
        .signal_semas = transfer_sema,
    });

    return true;
}

auto WorldRenderer::draw_profiler_ui() -> void {
    ZoneScoped;

    impl->pbr_graph.draw_profiler_ui();
}

auto WorldRenderer::sun_angle() -> glm::vec2 & {
    ZoneScoped;

    return impl->context.sun_angle;
}

auto WorldRenderer::update_sun_dir() -> void {
    ZoneScoped;

    auto rad = glm::radians(impl->context.sun_angle);
    this->world_data().sun.direction = {
        glm::cos(rad.x) * glm::sin(rad.y),
        glm::sin(rad.x) * glm::sin(rad.y),
        glm::cos(rad.y),
    };
}

auto WorldRenderer::world_data() -> GPUWorldData & {
    return impl->context.world_data;
}

auto WorldRenderer::composition_image() -> Image {
    return impl->frame_resources ? impl->frame_resources->composition_result : Image{};
}

}  // namespace lr
