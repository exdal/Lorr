#include "Engine/World/WorldRenderer.hh"

#include "Engine/Core/Application.hh"

#include "Engine/World/Tasks/ImGui.hh"

#include <glm/gtx/component_wise.hpp>

namespace lr {
template<>
struct Handle<WorldRenderer>::Impl {
    Device *device = nullptr;
    WorldRenderContext context = {};

    Sampler linear_sampler = {};
    Sampler nearest_sampler = {};

    Image imgui_font_image = {};
    ImageView imgui_font_view = {};
    Pipeline imgui_pipeline = {};
    std::vector<vuk::Value<vuk::SampledImage>> imgui_rendering_images = {};

    Pipeline grid_pipeline = {};

    Image sky_transmittance_lut = {};
    ImageView sky_transmittance_lut_view = {};
    Image sky_multiscatter_lut = {};
    ImageView sky_multiscatter_lut_view = {};
    Image sky_view_lut = {};
    ImageView sky_view_lut_view = {};
    Pipeline sky_view_pipeline = {};
    Image sky_aerial_perspective_lut = {};
    ImageView sky_aerial_perspective_lut_view = {};
    Pipeline sky_aerial_perspective_pipeline = {};
    Pipeline sky_final_pipeline = {};

    ls::option<vuk::Value<vuk::ImageAttachment>> composition_result = {};
};

auto WorldRenderer::create(Device *device) -> WorldRenderer {
    auto impl = new Impl;
    auto self = WorldRenderer(impl);
    impl->device = device;

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
    auto &transfer_man = app.device.transfer_man();
    auto shaders_root = asset_man.asset_root_path(AssetType::Shader);

    // clang-format off
    impl->linear_sampler = Sampler::create(
        *impl->device,
        vuk::Filter::eLinear,
        vuk::Filter::eLinear,
        vuk::SamplerMipmapMode::eLinear,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::SamplerAddressMode::eClampToEdge,
        vuk::CompareOp::eNever)
        .value();
    impl->nearest_sampler = Sampler::create(
        *impl->device,
        vuk::Filter::eNearest,
        vuk::Filter::eNearest,
        vuk::SamplerMipmapMode::eNearest,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::SamplerAddressMode::eRepeat,
        vuk::CompareOp::eNever)
        .value();

    //  ── IMGUI ───────────────────────────────────────────────────────────
    auto [imgui_atlas_data, imgui_atlas_dimensions] = Tasks::imgui_build_font_atlas(asset_man);
    impl->imgui_font_image = Image::create(
        *impl->device,
        vuk::Format::eR8G8B8A8Unorm,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageType::e2D,
        vuk::Extent3D(imgui_atlas_dimensions.x, imgui_atlas_dimensions.y, 1u))
        .value();
    impl->imgui_font_view = ImageView::create(
        *impl->device,
        impl->imgui_font_image,
        vuk::ImageUsageFlagBits::eSampled,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    transfer_man.upload_staging(impl->imgui_font_view, imgui_atlas_data, vuk::Access::eFragmentSampled);
    impl->imgui_pipeline = Pipeline::create(*impl->device, {
        .module_name = "imgui",
        .root_path = shaders_root,
        .shader_path = shaders_root / "imgui.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    //  ── GRID ────────────────────────────────────────────────────────────
    impl->grid_pipeline = Pipeline::create(*impl->device, {
        .module_name = "editor.grid",
        .root_path = shaders_root,
        .shader_path = shaders_root / "editor" / "grid.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    //  ── SKY ─────────────────────────────────────────────────────────────
    impl->sky_transmittance_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(256, 64, 1))
        .value();
    impl->sky_transmittance_lut_view = ImageView::create(
        *impl->device,
        impl->sky_transmittance_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->sky_multiscatter_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e2D,
        vuk::Extent3D(32, 32, 1))
        .value();
    impl->sky_multiscatter_lut_view = ImageView::create(
        *impl->device,
        impl->sky_multiscatter_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->sky_view_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        vuk::ImageType::e2D,
        vuk::Extent3D(200, 100, 1))
        .value();
    impl->sky_view_lut_view = ImageView::create(
        *impl->device,
        impl->sky_view_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eColorAttachment,
        vuk::ImageViewType::e2D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->sky_view_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.lut",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "lut.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();
    impl->sky_aerial_perspective_lut = Image::create(
        *impl->device,
        vuk::Format::eR16G16B16A16Sfloat,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageType::e3D,
        vuk::Extent3D(32, 32, 32))
        .value();
    impl->sky_aerial_perspective_lut_view = ImageView::create(
        *impl->device,
        impl->sky_aerial_perspective_lut,
        vuk::ImageUsageFlagBits::eSampled | vuk::ImageUsageFlagBits::eStorage,
        vuk::ImageViewType::e3D,
        { vuk::ImageAspectFlagBits::eColor })
        .value();
    impl->sky_aerial_perspective_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.aerial_perspective",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "aerial_perspective.slang",
        .entry_points = { "cs_main" },
    }).value();
    impl->sky_final_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.final",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "final.slang",
        .entry_points = { "vs_main", "fs_main" },
    }).value();

    auto sky_transmittance_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.transmittance",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "transmittance.slang",
        .entry_points = { "cs_main" },
    }).value();
    auto sky_multiscatter_pipeline = Pipeline::create(*impl->device, {
        .module_name = "atmos.ms",
        .root_path = shaders_root,
        .shader_path = shaders_root / "atmos" / "ms.slang",
        .entry_points = { "cs_main" },
    }).value();
    // clang-format on

    //  ── DEFINE PASSES ───────────────────────────────────────────────────
    auto transmittance_lut_pass = vuk::make_pass(
        "transmittance lut",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(sky_transmittance_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eComputeRW) dst) {
            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec2 image_size = {};
            };

            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_image(0, 0, dst)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = ctx.world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return dst;
        },
        vuk::DomainFlagBits::eGraphicsQueue);

    auto multiscatter_lut_pass = vuk::make_pass(
        "multiscatter lut",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(sky_multiscatter_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            [[maybe_unused]] VUK_IA(vuk::Access::eComputeSampled) transmittance_lut,
            VUK_IA(vuk::Access::eComputeRW) dst) {
            struct PushConstants {
                u64 world_ptr = 0;
                glm::vec2 image_size = {};
            };

            auto image_size = glm::vec2(dst->extent.width, dst->extent.height);
            auto groups = (glm::uvec2(image_size) / glm::uvec2(16)) + glm::uvec2(1);
            cmd_list  //
                .bind_compute_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, dst)
                .push_constants(
                    vuk::ShaderStageFlagBits::eCompute,
                    0,
                    PushConstants{
                        .world_ptr = ctx.world_ptr,
                        .image_size = image_size,
                    })
                .dispatch(groups.x, groups.y, 1);

            return std::make_tuple(transmittance_lut, dst);
        },
        vuk::DomainFlagBits::eGraphicsQueue);
    //   ──────────────────────────────────────────────────────────────────────

    auto transmittance_lut_attachment =
        impl->sky_transmittance_lut_view.discard(*impl->device, "sky_transmittance_lut", vuk::ImageUsageFlagBits::eStorage);
    auto multiscatter_lut_attachment =
        impl->sky_multiscatter_lut_view.discard(*impl->device, "sky_multiscatter_lut", vuk::ImageUsageFlagBits::eStorage);

    auto temp_scene_info = this->begin_scene({});
    this->end_scene(temp_scene_info);

    transmittance_lut_attachment = transmittance_lut_pass(std::move(transmittance_lut_attachment));
    std::tie(transmittance_lut_attachment, multiscatter_lut_attachment) =
        multiscatter_lut_pass(std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));
    transmittance_lut_attachment.release(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);
    multiscatter_lut_attachment.release(vuk::Access::eFragmentSampled, vuk::DomainFlagBits::eGraphicsQueue);

    transfer_man.wait_on(std::move(transmittance_lut_attachment));
    transfer_man.wait_on(std::move(multiscatter_lut_attachment));
}

auto WorldRenderer::begin_scene(const SceneBeginInfo &info) -> GPUSceneData {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();
    GPUSceneData result;

    usize world_data_bytes = sizeof(GPUWorldData);
    usize cameras_size_bytes = sizeof(GPUCameraData) * info.camera_count;
    usize model_transforms_bytes = sizeof(GPUModelTransformData) * info.model_transform_count;
    usize buffer_size = world_data_bytes + cameras_size_bytes + model_transforms_bytes;

    result.world_data = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eCPUonly, buffer_size);
    result.upload_size += sizeof(GPUWorldData);
    auto ptr = result.world_data.host_ptr();

    if (info.camera_count != 0) {
        result.cameras = reinterpret_cast<GPUCameraData *>(ptr + result.upload_size);

        result.offset_cameras = result.upload_size;
        result.upload_size += cameras_size_bytes;
    }

    if (info.model_transform_count != 0) {
        result.model_transforms = reinterpret_cast<GPUModelTransformData *>(ptr + result.upload_size);

        result.offset_model_transforms = result.upload_size;
        result.upload_size += model_transforms_bytes;
    }

    LS_EXPECT(result.upload_size == buffer_size);

    return result;
}

auto WorldRenderer::end_scene(GPUSceneData &scene_gpu_data) -> void {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();
    auto gpu_buffer = transfer_man.alloc_transient_buffer(vuk::MemoryUsage::eGPUonly, scene_gpu_data.upload_size);
    auto materials_buffer_ptr = scene_gpu_data.materials_buffer_id != BufferID::Invalid
                                    ? impl->device->buffer(scene_gpu_data.materials_buffer_id)->device_address
                                    : 0;

    auto &world_data = impl->context.world_data;
    world_data.linear_sampler_id = impl->linear_sampler.id();
    world_data.nearest_sampler_id = impl->nearest_sampler.id();

    if (scene_gpu_data.offset_cameras.has_value()) {
        world_data.cameras_ptr = gpu_buffer.device_address() + scene_gpu_data.offset_cameras.value();
    }
    world_data.materials_ptr = materials_buffer_ptr;
    if (scene_gpu_data.offset_model_transforms.has_value()) {
        world_data.model_transforms_ptr = gpu_buffer.device_address() + scene_gpu_data.offset_model_transforms.value();
    }
    world_data.active_camera_index = scene_gpu_data.active_camera_index;

    impl->context.world_ptr = gpu_buffer.device_address();
    std::memcpy(scene_gpu_data.world_data.host_ptr<GPUWorldData>(), &impl->context.world_data, sizeof(GPUWorldData));

    transfer_man.upload_staging(scene_gpu_data.world_data, gpu_buffer);
}

auto WorldRenderer::begin_frame([[maybe_unused]] vuk::Value<vuk::ImageAttachment> &reference_img) -> void {
    ZoneScoped;

    impl->imgui_rendering_images.clear();

    ImGui::NewFrame();
}

auto WorldRenderer::end_frame(vuk::Value<vuk::ImageAttachment> &&render_target) -> vuk::Value<vuk::ImageAttachment> {
    ZoneScoped;

    auto &transfer_man = impl->device->transfer_man();

    auto final_attachment = vuk::declare_ia("final");
    final_attachment.similar_to(render_target);
    final_attachment->format = vuk::Format::eR16G16B16A16Sfloat;

    //  ── SKY VIEW LUT ────────────────────────────────────────────────────
    auto sky_view_pass = vuk::make_pass(
        "sky view",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->sky_view_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut,
            VUK_IA(vuk::Access::eFragmentSampled) multiscatter_lut) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, multiscatter_lut)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, ctx.world_ptr)
                .draw(3, 1, 0, 0);
            return std::make_tuple(dst, transmittance_lut, multiscatter_lut);
        });

    auto transmittance_lut_attachment = impl->sky_transmittance_lut_view.acquire(
        *impl->device, "transmittance_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
    auto multiscatter_lut_attachment = impl->sky_multiscatter_lut_view.acquire(
        *impl->device, "multiscatter_lut", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);

    auto sky_view_lut_attachment = vuk::declare_ia("sky_view_lut");
    sky_view_lut_attachment.similar_to(final_attachment);
    sky_view_lut_attachment->extent = { 200, 100, 1 };
    sky_view_lut_attachment = vuk::clear_image(std::move(sky_view_lut_attachment), vuk::Black<f32>);
    std::tie(sky_view_lut_attachment, transmittance_lut_attachment, multiscatter_lut_attachment) =
        sky_view_pass(std::move(sky_view_lut_attachment), std::move(transmittance_lut_attachment), std::move(multiscatter_lut_attachment));

    //  ── SKY FINAL ───────────────────────────────────────────────────────
    auto sky_final_pass = vuk::make_pass(
        "sky final",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->sky_final_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_IA(vuk::Access::eFragmentSampled) sky_view_lut,
            VUK_IA(vuk::Access::eFragmentSampled) transmittance_lut) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .bind_sampler(0, 0, { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })
                .bind_image(0, 1, transmittance_lut)
                .bind_image(0, 2, sky_view_lut)
                .set_rasterization({})
                .set_depth_stencil({})
                .set_color_blend(dst, vuk::BlendPreset::eOff)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eFragment, 0, ctx.world_ptr)
                .draw(3, 1, 0, 0);
            return dst;
        });

    final_attachment = vuk::clear_image(std::move(final_attachment), vuk::Black<f32>);
    final_attachment =
        sky_final_pass(std::move(final_attachment), std::move(sky_view_lut_attachment), std::move(transmittance_lut_attachment));

    //  ── EDITOR GRID ─────────────────────────────────────────────────────
    auto editor_grid_pass = vuk::make_pass(
        "editor grid",
        [&ctx = impl->context, &pipeline = *impl->device->pipeline(impl->grid_pipeline.id())](
            vuk::CommandBuffer &cmd_list, VUK_IA(vuk::Access::eColorWrite) dst) {
            cmd_list  //
                .bind_graphics_pipeline(pipeline)
                .set_rasterization({ .cullMode = vuk::CullModeFlagBits::eNone })
                .set_depth_stencil({ .depthTestEnable = true, .depthWriteEnable = true, .depthCompareOp = vuk::CompareOp::eLessOrEqual })
                .set_color_blend(dst, vuk::BlendPreset::eAlphaBlend)
                .set_viewport(0, vuk::Rect2D::framebuffer())
                .set_scissor(0, vuk::Rect2D::framebuffer())
                .push_constants(vuk::ShaderStageFlagBits::eVertex | vuk::ShaderStageFlagBits::eFragment, 0, ctx.world_ptr)
                .draw(3, 1, 0, 0);
            return dst;
        });

    final_attachment = editor_grid_pass(std::move(final_attachment));

    //   ──────────────────────────────────────────────────────────────────────
    // Store result before rendering imgui
    // TODO: Find a better way to check your shit
    if (impl->context.world_data.cameras_ptr != 0) {
        impl->composition_result = std::move(final_attachment);
    }
    //  ── IMGUI ───────────────────────────────────────────────────────────
    ImGui::Render();

    ImDrawData *draw_data = ImGui::GetDrawData();
    u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
    u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
    if (!draw_data || vertex_size_bytes == 0) {
        return std::move(render_target);
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
         font_atlas_view = impl->device->image_view(impl->imgui_font_view.id()),
         vertices = vertex_buffer.buffer,
         indices = index_buffer.buffer,
         &pipeline = *impl->device->pipeline(impl->imgui_pipeline.id())](
            vuk::CommandBuffer &cmd_list,
            VUK_IA(vuk::Access::eColorWrite) dst,
            VUK_ARG(vuk::SampledImage[], vuk::Access::eFragmentSampled) sampled_images) {
            struct PushConstants {
                glm::vec2 translate = {};
                glm::vec2 scale = {};
            };

            PushConstants c;
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

                        if (im_cmd.TextureId) {
                            auto index = reinterpret_cast<usize>(im_cmd.TextureId) - 1;
                            cmd_list  //
                                .bind_sampler(0, 0, sampled_images[index].sci)
                                .bind_image(0, 1, sampled_images[index].ia);
                        } else {
                            cmd_list  //
                                .bind_sampler(0, 0, {})
                                .bind_image(0, 1, *font_atlas_view);
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
    imgui_io.Fonts->TexID = reinterpret_cast<ImTextureID>(impl->imgui_rendering_images.size() + 1);
    auto imgui_font_attachment =
        impl->imgui_font_view.acquire(*impl->device, "imgui_font_atlas", vuk::ImageUsageFlagBits::eSampled, vuk::Access::eFragmentSampled);
    impl->imgui_rendering_images.emplace_back(vuk::combine_image_sampler(
        "imgui_font_atlas",
        std::move(imgui_font_attachment),
        vuk::acquire_sampler("imgui_font_sampler", { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })));
    auto imgui_rendering_images_arr = vuk::declare_array("imgui_rendering_images", std::span(impl->imgui_rendering_images));

    render_target = imgui_pass(std::move(render_target), std::move(imgui_rendering_images_arr));

    return render_target;
}

auto WorldRenderer::draw_profiler_ui() -> void {
    ZoneScoped;
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

auto WorldRenderer::composition_result() -> ls::option<vuk::Value<vuk::ImageAttachment>> {
    return impl->composition_result;
}

auto WorldRenderer::imgui_image(vuk::Value<vuk::ImageAttachment> &&attachment, const glm::vec2 &size) -> void {
    ZoneScoped;

    impl->imgui_rendering_images.emplace_back(vuk::combine_image_sampler(
        "_imgui_img",
        std::move(attachment),
        vuk::acquire_sampler("_imgui_sampler", { .magFilter = vuk::Filter::eLinear, .minFilter = vuk::Filter::eLinear })));

    // make it +1 so it wont be nullptr, we already decrement it by one in imgui pass
    auto im_texture_id = reinterpret_cast<ImTextureID>(impl->imgui_rendering_images.size());
    ImGui::Image(im_texture_id, { size.x, size.y });
}

}  // namespace lr
