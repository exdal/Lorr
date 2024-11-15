#include "Engine/World/Renderer.hh"

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
WorldRenderer::WorldRenderer(Device device_)
    : device(device_) {
    ZoneScoped;

    this->transfer_man = TransferManager::create_with_gpu(device_, ls::mib_to_bytes(8), vk::BufferUsage::Storage);
    this->pbr_graph = TaskGraph::create({
        .device = device_,
        .staging_buffer_size = ls::mib_to_bytes(16),
        .staging_buffer_uses = vk::BufferUsage::Vertex | vk::BufferUsage::Index,
    });

    this->setup_persistent_resources();
}

auto WorldRenderer::setup_persistent_resources(this WorldRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    self.linear_sampler = Sampler::create(
                              self.device,
                              vk::Filtering::Linear,
                              vk::Filtering::Linear,
                              vk::Filtering::Linear,
                              vk::SamplerAddressMode::ClampToEdge,
                              vk::SamplerAddressMode::ClampToEdge,
                              vk::SamplerAddressMode::ClampToEdge,
                              vk::CompareOp::Never)
                              .value();
    self.nearest_sampler = Sampler::create(
                               self.device,
                               vk::Filtering::Nearest,
                               vk::Filtering::Nearest,
                               vk::Filtering::Nearest,
                               vk::SamplerAddressMode::Repeat,
                               vk::SamplerAddressMode::Repeat,
                               vk::SamplerAddressMode::Repeat,
                               vk::CompareOp::Never)
                               .value();

    auto [imgui_atlas_data, imgui_atlas_dimensions] = Tasks::imgui_build_font_atlas(asset_man);
    self.imgui_font_image = Image::create(
                                self.device,
                                vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
                                vk::Format::R8G8B8A8_UNORM,
                                vk::ImageType::View2D,
                                vk::Extent3D(imgui_atlas_dimensions.x, imgui_atlas_dimensions.y, 1u))
                                .value();
    self.device.upload(self.imgui_font_image, imgui_atlas_data, glm::compMul(imgui_atlas_dimensions) * 4, vk::ImageLayout::ShaderReadOnly);

    self.sky_transmittance_image = Image::create(
                                       self.device,
                                       vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
                                       vk::Format::R16G16B16A16_SFLOAT,
                                       vk::ImageType::View2D,
                                       { 256, 64, 1 })
                                       .value();
    self.device.image_transition(self.sky_transmittance_image, vk::ImageLayout::Undefined, vk::ImageLayout::General);

    self.sky_multiscatter_image = Image::create(
                                      self.device,
                                      vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
                                      vk::Format::R16G16B16A16_SFLOAT,
                                      vk::ImageType::View2D,
                                      { 32, 32, 1 })  // dont change
                                      .value();
    self.device.image_transition(self.sky_multiscatter_image, vk::ImageLayout::Undefined, vk::ImageLayout::General);

    self.sky_aerial_perspective_image = Image::create(
                                            self.device,
                                            vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
                                            vk::Format::R16G16B16A16_SFLOAT,
                                            vk::ImageType::View3D,
                                            { 32, 32, 32 })
                                            .value();
    self.device.image_transition(self.sky_aerial_perspective_image, vk::ImageLayout::Undefined, vk::ImageLayout::General);

    self.sky_view_image = Image::create(
                              self.device,
                              vk::ImageUsage::ColorAttachment | vk::ImageUsage::Sampled,
                              vk::Format::R16G16B16A16_SFLOAT,
                              vk::ImageType::View2D,
                              { 200, 100, 1 })
                              .value();
    self.device.image_transition(self.sky_view_image, vk::ImageLayout::Undefined, vk::ImageLayout::ColorAttachment);

    self.cloud_shape_noise_image = Image::create(
                                       self.device,
                                       vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
                                       vk::Format::R8G8B8A8_UNORM,
                                       vk::ImageType::View3D,
                                       { 256, 256, 256 })
                                       .value();
    self.device.image_transition(self.cloud_shape_noise_image, vk::ImageLayout::Undefined, vk::ImageLayout::General);

    self.cloud_detail_noise_image = Image::create(
                                        self.device,
                                        vk::ImageUsage::Storage | vk::ImageUsage::Sampled,
                                        vk::Format::R8G8B8A8_UNORM,
                                        vk::ImageType::View3D,
                                        { 128, 128, 128 })
                                        .value();
    self.device.image_transition(self.cloud_detail_noise_image, vk::ImageLayout::Undefined, vk::ImageLayout::General);
}

auto WorldRenderer::record_setup_graph(this WorldRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto task_graph = TaskGraph::create({ .device = self.device });

    auto sky_transmittance_pipeline = Pipeline::create(self.device, Tasks::sky_transmittance_pipeline_info(asset_man)).value();
    auto sky_transmittance_task_image = task_graph.add_image({
        .image_id = self.sky_transmittance_image,
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

            tc.set_pipeline(sky_transmittance_pipeline);
            tc.set_push_constants(PushConstants{
                .world_ptr = render_context.world_ptr,
                .image_size = { lut_image.extent().width, lut_image.extent().height },
                .image_view_id = lut_image.view(),
            });
            tc.cmd_list.dispatch(lut_image.extent().width / 16 + 1, lut_image.extent().height / 16 + 1, 1);
        }
    });

    auto sky_multiscatter_pipeline = Pipeline::create(self.device, Tasks::sky_multiscatter_pipeline_info(asset_man)).value();
    auto sky_multiscatter_task_image = task_graph.add_image({
        .image_id = self.sky_multiscatter_image,
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

            tc.set_pipeline(sky_multiscatter_pipeline);
            tc.set_push_constants(PushConstants{
                .world_ptr = render_context.world_ptr,
                .transmittance_image_id = transmittance_lut_image.view(),
                .ms_image_size = { ms_lut_image.extent().width, ms_lut_image.extent().height },
                .ms_image_id = ms_lut_image.view(),
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

    auto cloud_shape_noise_pipeline = Pipeline::create(self.device, Tasks::cloud_shape_noise_pipeline_info(asset_man)).value();
    auto cloud_shape_noise_task_image = task_graph.add_image({
        .image_id = self.cloud_shape_noise_image,
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

            tc.set_pipeline(cloud_shape_noise_pipeline);
            tc.set_push_constants(PushConstants{
                .image_size = glm::vec3(1.0) / glm::vec3(f32(image_extent.width), f32(image_extent.height), image_extent.depth),
                .detail_image_view = cloud_noise_image_info.view(),
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

    auto cloud_detail_noise_pipeline = Pipeline::create(self.device, Tasks::cloud_detail_noise_pipeline_info(asset_man)).value();
    auto cloud_detail_noise_task_image = task_graph.add_image({
        .image_id = self.cloud_detail_noise_image,
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

            tc.set_pipeline(cloud_detail_noise_pipeline);
            tc.set_push_constants(PushConstants{
                .image_size = glm::vec3(1.0) / glm::vec3(f32(image_extent.width), f32(image_extent.height), image_extent.depth),
                .detail_image_view = cloud_noise_image_info.view(),
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

    auto transfer_sema = self.device.queue(vk::CommandType::Transfer).semaphore();
    task_graph.execute({
        .frame_index = 0,
        .execution_data = &self.context.value(),
        .wait_semas = transfer_sema,
        .signal_semas = transfer_sema,
    });
}

auto WorldRenderer::record_pbr_graph(this WorldRenderer &self, SwapChain &swap_chain) -> void {
    ZoneScoped;

    auto &asset_man = Application::get().asset_man;
    self.swap_chain_image = self.pbr_graph.add_image({});

    self.editor_view_image = Image::create(
                                 self.device,
                                 vk::ImageUsage::ColorAttachment | vk::ImageUsage::Sampled,
                                 vk::Format::B8G8R8A8_SRGB,
                                 vk::ImageType::View2D,
                                 swap_chain.extent())
                                 .value();
    self.device.image_transition(self.editor_view_image, vk::ImageLayout::Undefined, vk::ImageLayout::ColorAttachment);

    self.final_image = Image::create(
                           self.device,
                           vk::ImageUsage::ColorAttachment | vk::ImageUsage::Sampled,
                           vk::Format::R32G32B32A32_SFLOAT,
                           vk::ImageType::View2D,
                           swap_chain.extent())
                           .value();
    self.device.image_transition(self.final_image, vk::ImageLayout::Undefined, vk::ImageLayout::ColorAttachment);

    self.geo_depth_image = Image::create(
                               self.device,
                               vk::ImageUsage::DepthStencilAttachment | vk::ImageUsage::Sampled,
                               vk::Format::D32_SFLOAT,
                               vk::ImageType::View2D,
                               swap_chain.extent(),
                               vk::ImageAspectFlag::Depth)
                               .value();
    self.device.image_transition(self.geo_depth_image, vk::ImageLayout::Undefined, vk::ImageLayout::DepthAttachment);

    auto editor_view_task_image = self.pbr_graph.add_image({
        .image_id = self.editor_view_image,
        .layout = vk::ImageLayout::ColorAttachment,
        .access = vk::PipelineAccess::ColorAttachmentWrite,
    });

    auto final_task_image = self.pbr_graph.add_image({
        .image_id = self.final_image,
        .layout = vk::ImageLayout::ColorAttachment,
        .access = vk::PipelineAccess::ColorAttachmentWrite,
    });

    auto geo_depth_task_image = self.pbr_graph.add_image({
        .image_id = self.geo_depth_image,
        .layout = vk::ImageLayout::DepthAttachment,
        .access = vk::PipelineAccess::DepthStencilWrite,
    });

    auto sky_transmittance_task_image = self.pbr_graph.add_image({
        .image_id = self.sky_transmittance_image,
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    auto sky_multiscatter_task_image = self.pbr_graph.add_image({
        .image_id = self.sky_multiscatter_image,
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    auto sky_aerial_perspective_task_image = self.pbr_graph.add_image({
        .image_id = self.sky_aerial_perspective_image,
        .layout = vk::ImageLayout::General,
        .access = vk::PipelineAccess::ComputeWrite,
    });

    auto sky_view_task_image = self.pbr_graph.add_image({
        .image_id = self.sky_view_image,
        .layout = vk::ImageLayout::ColorAttachment,
        .access = vk::PipelineAccess::ColorAttachmentWrite,
    });

    auto cloud_shape_noise_task_image = self.pbr_graph.add_image({
        .image_id = self.cloud_shape_noise_image,
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    auto cloud_detail_noise_task_image = self.pbr_graph.add_image({
        .image_id = self.cloud_detail_noise_image,
        .layout = vk::ImageLayout::ShaderReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });

    self.pbr_graph.add_task(Tasks::SkyViewTask{
        .uses = {
            .sky_view_attachment = sky_view_task_image,
            .transmittance_lut = sky_transmittance_task_image,
            .ms_lut = sky_multiscatter_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::sky_view_pipeline_info(asset_man,
                    self.device.image(self.sky_view_image).format())).value(),
    });
    self.pbr_graph.add_task(Tasks::SkyAerialTask{
        .uses = {
            .aerial_lut = sky_aerial_perspective_task_image,
            .transmittance_lut = sky_transmittance_task_image,
            .multiscatter_lut = sky_multiscatter_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::sky_aerial_pipeline_info(asset_man)).value(),
    });
    self.pbr_graph.add_task(Tasks::SkyFinalTask{
        .uses = {
            .color_attachment = final_task_image,
            .sky_view_lut = sky_view_task_image,
            .transmittance_lut = sky_transmittance_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::sky_final_pipeline_info(asset_man,
                    self.device.image(self.final_image).format())).value(),
        .sky_sampler = Sampler::create(
            self.device,
            vk::Filtering::Linear,
            vk::Filtering::Linear,
            vk::Filtering::Linear,
            vk::SamplerAddressMode::ClampToEdge,
            vk::SamplerAddressMode::ClampToEdge,
            vk::SamplerAddressMode::ClampToEdge,
            vk::CompareOp::Never).value(),
    });
    self.pbr_graph.add_task(Tasks::CloudDraw{
        .uses = {
            .color_attachment = final_task_image,
            .depth_attachment = geo_depth_task_image,
            .transmittance_lut_image = sky_transmittance_task_image,
            .aerial_perspective_lut_image = sky_aerial_perspective_task_image,
            .cloud_shape_image = cloud_shape_noise_task_image,
            .cloud_detail_image = cloud_detail_noise_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::draw_clouds_pipeline_info(asset_man,
                    self.device.image(self.final_image).format())).value(),
        .sampler = Sampler::create(
            self.device,
            vk::Filtering::Linear,
            vk::Filtering::Linear,
            vk::Filtering::Linear,
            vk::SamplerAddressMode::Repeat,
            vk::SamplerAddressMode::Repeat,
            vk::SamplerAddressMode::Repeat,
            vk::CompareOp::Never).value(),
    });

    self.pbr_graph.add_task(Tasks::GeometryTask{
        .uses = {
            .color_attachment = final_task_image,
            .depth_attachment = geo_depth_task_image,
            .transmittance_image = sky_transmittance_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::geometry_pipeline_info(asset_man,
                    self.device.image(self.final_image).format(), self.device.image(self.geo_depth_image).format())).value(),
    });
    self.pbr_graph.add_task(Tasks::SkyApplyAerialTask{
        .uses = {
            .color_attachment = final_task_image,
            .depth_image = geo_depth_task_image,
            .aerial_perspective_image = sky_aerial_perspective_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::sky_apply_aerial_pipeline_info(asset_man,
                    self.device.image(self.final_image).format())).value(),
    });
    self.pbr_graph.add_task(Tasks::GridTask{
        .uses = {
            .color_attachment = final_task_image,
            .depth_attachment = geo_depth_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::grid_pipeline_info(asset_man,
                    self.device.image(self.final_image).format(), self.device.image(self.geo_depth_image).format())).value(),
    });
    self.pbr_graph.add_task(Tasks::TonemapTask{
        .uses = {
            .dst_attachment = editor_view_task_image,
            .src_image = final_task_image,
        },
        .pipeline = Pipeline::create(self.device, Tasks::tonemap_pipeline_info(asset_man,
                    self.device.image(self.editor_view_image).format())).value()
    });
    self.pbr_graph.add_task(Tasks::ImGuiTask{
        .uses = {
            .attachment = self.swap_chain_image,
        },
        .font_atlas_image = self.imgui_font_image,
        .pipeline = Pipeline::create(self.device, Tasks::imgui_pipeline_info(asset_man,
                    swap_chain.format())).value()
    });
    self.pbr_graph.present(self.swap_chain_image);
}

auto WorldRenderer::construct_scene(this WorldRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &world = app.world;
    auto &scene = world.scene_at(world.active_scene.value());
    auto &asset_man = app.asset_man;
    auto transfer_man_sema = self.transfer_man.semaphore();
    auto transfer_queue = self.device.queue(vk::CommandType::Transfer);

    auto model_transform_query =  //
        world.ecs
            .query_builder<Component::Transform, Component::RenderableModel>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();

    // Create tightly packed vertex/index buffer ignoring all individual alignments.
    //
    struct ModelBufferInfo {
        BufferID buffer_id = BufferID::Invalid;
        u32 element_count = 0;
    };

    std::vector<ModelBufferInfo> vertex_buffers_cpu = {};
    vertex_buffers_cpu.reserve(model_transform_query.count());
    std::vector<ModelBufferInfo> index_buffers_cpu = {};
    index_buffers_cpu.reserve(model_transform_query.count());

    u32 total_vertex_count = 0;
    u32 total_index_count = 0;

    self.gpu_mesh_primitives.clear();
    self.gpu_meshes.clear();
    self.gpu_models.clear();

    std::vector<Identifier> gpu_material_idents = {};
    std::vector<GPUMaterial> gpu_materials = {};

    std::vector<Identifier> gpu_model_idents = {};
    model_transform_query.each([&](Component::Transform &, Component::RenderableModel &m) {
        u32 model_id = 0;
        for (; model_id < gpu_model_idents.size(); model_id++) {
            if (m.identifier == gpu_model_idents[model_id]) {
                break;
            }
        }
        bool model_exists = !(model_id == gpu_model_idents.size()) && !gpu_model_idents.empty();
        if (!model_exists) {
            auto *model = asset_man.model(m.identifier);

            std::vector<GPUMeshPrimitive> cur_gpu_mesh_primitives = {};
            cur_gpu_mesh_primitives.reserve(model->primitives.size());
            for (auto &v : model->primitives) {
                u32 material_id = 0;
                for (; material_id < gpu_material_idents.size(); material_id++) {
                    if (v.material_ident == gpu_material_idents[material_id]) {
                        break;
                    }
                }
                bool material_exists = !(material_id == gpu_material_idents.size()) && !gpu_material_idents.empty();
                if (!material_exists) {
                    auto *material = asset_man.material(v.material_ident);
                    auto *albedo_texture = asset_man.texture(material->albedo_texture_ident.value_or(INVALID_TEXTURE));
                    auto albedo_image = self.device.image(albedo_texture->image_id);
                    auto *normal_texture = asset_man.texture(material->normal_texture_ident.value_or(INVALID_TEXTURE));
                    auto normal_image = self.device.image(normal_texture->image_id);
                    auto *emissive_texture = asset_man.texture(material->emissive_texture_ident.value_or(INVALID_TEXTURE));
                    auto emissive_image = self.device.image(emissive_texture->image_id);

                    material_id = gpu_materials.size();
                    gpu_material_idents.push_back(v.material_ident);
                    gpu_materials.push_back({
                        .albedo_color = material->albedo_color,
                        .emissive_color = material->emissive_color,
                        .roughness_factor = material->roughness_factor,
                        .metallic_factor = material->metallic_factor,
                        .alpha_mode = material->alpha_mode,
                        .alpha_cutoff = material->alpha_cutoff,
                        .albedo_image = { albedo_image.view(), albedo_texture->sampler_id },
                        .normal_image = { normal_image.view(), normal_texture->sampler_id },
                        .emissive_image = { emissive_image.view(), emissive_texture->sampler_id },
                    });
                }

                cur_gpu_mesh_primitives.push_back({
                    .vertex_offset = total_vertex_count,
                    .vertex_count = v.vertex_count,
                    .index_offset = total_index_count,
                    .index_count = v.index_count,
                    .material_id = static_cast<GPUMaterialID>(material_id),
                });

                total_vertex_count += v.vertex_count;
                total_index_count += v.index_count;
            }

            u32 latest_mesh_index = self.gpu_meshes.size();
            std::vector<u32> cur_gpu_mesh_indices = {};
            cur_gpu_mesh_indices.reserve(model->meshes.size());

            u32 latest_primitive_index = self.gpu_mesh_primitives.size();
            std::vector<u32> cur_gpu_mesh_primitive_indices = {};
            std::vector<GPUMesh> cur_gpu_meshes = {};
            cur_gpu_mesh_primitives.reserve(model->primitives.size());
            cur_gpu_meshes.reserve(model->meshes.size());
            for (auto &v : model->meshes) {
                for (auto &p : v.primitive_indices) {
                    cur_gpu_mesh_primitive_indices.push_back(latest_primitive_index + p);
                }

                cur_gpu_meshes.push_back({
                    .primitive_indices = std::move(cur_gpu_mesh_primitive_indices),
                });

                vertex_buffers_cpu.emplace_back(v.vertex_buffer_cpu, v.total_vertex_count);
                index_buffers_cpu.emplace_back(v.index_buffer_cpu, v.total_index_count);

                cur_gpu_mesh_indices.push_back(cur_gpu_mesh_indices.size() + latest_mesh_index);
            }

            gpu_model_idents.push_back(m.identifier);
            self.gpu_mesh_primitives.insert(self.gpu_mesh_primitives.end(), cur_gpu_mesh_primitives.begin(), cur_gpu_mesh_primitives.end());
            self.gpu_meshes.insert(self.gpu_meshes.end(), cur_gpu_meshes.begin(), cur_gpu_meshes.end());
            self.gpu_models.push_back({
                .mesh_indices = std::move(cur_gpu_mesh_indices),
            });
        }
    });

    auto cmd_list = transfer_queue.begin();
    usize gpu_material_buffer_size = gpu_materials.size() * sizeof(GPUMaterial);

    auto material_buffer_cpu = self.transfer_man.allocate(gpu_material_buffer_size).value();
    std::memcpy(material_buffer_cpu.ptr, gpu_materials.data(), gpu_material_buffer_size);

    self.material_buffer =
        Buffer::create(self.device, vk::BufferUsage::TransferDst, gpu_material_buffer_size, vk::MemoryAllocationUsage::PreferDevice)
            .value();
    vk::BufferCopyRegion material_buffer_region = {
        .src_offset = material_buffer_cpu.offset,
        .dst_offset = 0,
        .size = gpu_material_buffer_size,
    };
    cmd_list.copy_buffer_to_buffer(material_buffer_cpu.cpu_buffer_id, self.material_buffer, material_buffer_region);

    self.static_vertex_buffer = Buffer::create(
                                    self.device,
                                    vk::BufferUsage::TransferDst | vk::BufferUsage::Vertex,
                                    total_vertex_count * sizeof(Vertex),
                                    vk::MemoryAllocationUsage::PreferDevice)
                                    .value();
    self.device.buffer(self.static_vertex_buffer).set_name("Static Vertex Buffer");

    u64 vertex_buffer_offset = 0;
    for (auto &[buffer_id, element_count] : vertex_buffers_cpu) {
        vk::BufferCopyRegion copy_region = {
            .src_offset = 0,
            .dst_offset = vertex_buffer_offset,
            .size = element_count * sizeof(Vertex),
        };
        cmd_list.copy_buffer_to_buffer(buffer_id, self.static_vertex_buffer, copy_region);
        transfer_queue.defer(buffer_id);

        vertex_buffer_offset += copy_region.size;
    }

    self.static_index_buffer = Buffer::create(
                                   self.device,
                                   vk::BufferUsage::TransferDst | vk::BufferUsage::Index,
                                   total_index_count * sizeof(u32),
                                   vk::MemoryAllocationUsage::PreferDevice)
                                   .value();
    self.device.buffer(self.static_index_buffer).set_name("Static Index Buffer");

    u64 index_buffer_offset = 0;
    for (auto &[buffer_id, element_count] : index_buffers_cpu) {
        vk::BufferCopyRegion copy_region = {
            .src_offset = 0,
            .dst_offset = index_buffer_offset,
            .size = element_count * sizeof(u32),
        };
        cmd_list.copy_buffer_to_buffer(buffer_id, self.static_index_buffer, copy_region);
        transfer_queue.defer(buffer_id);

        index_buffer_offset += copy_region.size;
    }

    transfer_queue.end(cmd_list);
    transfer_queue.submit({}, transfer_man_sema);
    transfer_queue.wait();
    self.transfer_man.collect_garbage();
    transfer_queue.collect_garbage();
}

auto WorldRenderer::render(this WorldRenderer &self, SwapChain &swap_chain, ImageID image_id) -> bool {
    ZoneScoped;

    auto &app = Application::get();
    auto &world = app.world;
    auto transfer_sema = self.transfer_man.semaphore();
    if (!world.active_scene.has_value()) {
        return false;
    }

    auto &scene = world.scene_at(world.active_scene.value());
    bool has_world_data = self.context.has_value();

    GPUWorldData new_world_data = {};
    auto &world_data = has_world_data ? self.context.value().world_data : new_world_data;
    world_data.linear_sampler = self.linear_sampler;
    world_data.nearest_sampler = self.nearest_sampler;

    // Prepare world ECS context
    auto camera_query =  //
        world.ecs
            .query_builder<Component::Transform, Component::Camera>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();
    auto directional_light_query =  //
        world.ecs
            .query_builder<Component::DirectionalLight>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();
    auto model_transform_query =  //
        world.ecs
            .query_builder<Component::Transform, Component::RenderableModel>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();

    // Process entities
    directional_light_query.each([&](Component::DirectionalLight &l) {
        auto rad = glm::radians(l.angle);
        world_data.sun.direction = {
            glm::cos(rad.x) * glm::sin(rad.y),
            glm::sin(rad.x) * glm::sin(rad.y),
            glm::cos(rad.y),
        };
        world_data.sun.intensity = l.intensity;
    });

    // Prepare buffers
    self.transfer_man.collect_garbage();
    auto gpu_world_alloc = self.transfer_man.allocate(sizeof(GPUWorldData)).value();
    auto gpu_cameras_alloc = self.transfer_man.allocate(sizeof(GPUCameraData) * camera_query.count()).value();
    auto gpu_model_transforms_alloc = self.transfer_man.allocate(sizeof(GPUModelTransformData) * model_transform_query.count()).value();
    GPUAllocation all_buffers[] = { gpu_world_alloc, gpu_cameras_alloc, gpu_model_transforms_alloc };

    world_data.cameras_ptr = gpu_cameras_alloc.gpu_offset();
    auto camera_ptr = reinterpret_cast<GPUCameraData *>(gpu_cameras_alloc.ptr);
    camera_query.each([&](Component::Transform &t, Component::Camera &c) {
        camera_ptr->projection_mat = c.projection;
        camera_ptr->view_mat = t.matrix;
        camera_ptr->projection_view_mat = glm::transpose(c.projection * t.matrix);
        camera_ptr->inv_view_mat = glm::inverse(glm::transpose(t.matrix));
        camera_ptr->inv_projection_view_mat = glm::inverse(glm::transpose(c.projection)) * camera_ptr->inv_view_mat;
        camera_ptr->position = t.position;
        camera_ptr->near_clip = c.near_clip;
        camera_ptr->far_clip = c.far_clip;

        camera_ptr++;
    });

    if (self.material_buffer != BufferID::Invalid) {
        world_data.materials_ptr = self.device.buffer(self.material_buffer).device_address();
    }

    world_data.model_transforms_ptr = gpu_model_transforms_alloc.gpu_offset();
    auto model_transforms_ptr = reinterpret_cast<GPUModelTransformData *>(gpu_model_transforms_alloc.ptr);
    model_transform_query.each([&](Component::Transform &t, Component::RenderableModel &) {
        auto rotation = glm::radians(t.rotation);

        glm::mat4 &world_mat = model_transforms_ptr->world_transform_mat;
        world_mat = glm::translate(glm::mat4(1.0), glm::vec3(0.0f));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.x, glm::vec3(1.0, 0.0, 0.0));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.y, glm::vec3(0.0, 1.0, 0.0));
        world_mat *= glm::rotate(glm::mat4(1.0), rotation.z, glm::vec3(0.0, 0.0, 1.0));
        world_mat *= glm::scale(glm::mat4(1.0), t.scale);

        model_transforms_ptr->model_transform_mat = t.matrix;
        model_transforms_ptr++;
    });

    // ALWAYS PUT THIS TO END
    auto world_ptr = gpu_world_alloc.gpu_offset();
    std::memcpy(gpu_world_alloc.ptr, &world_data, sizeof(GPUWorldData));

    self.context = WorldRenderContext{
        .world_ptr = world_ptr,
        .world_data = world_data,
        .models = self.gpu_models,
        .meshes = self.gpu_meshes,
        .mesh_primitives = self.gpu_mesh_primitives,
        .vertex_buffer = self.static_vertex_buffer,
        .index_buffer = self.static_index_buffer,
    };

    self.transfer_man.upload(all_buffers);

    // Refresh the scene
    if (!has_world_data) {
        self.device.wait();
        self.record_setup_graph();
        self.record_pbr_graph(swap_chain);
    }

    self.pbr_graph.set_image(self.swap_chain_image, { .image_id = image_id });
    self.pbr_graph.execute(TaskExecuteInfo{
        .frame_index = swap_chain.image_index(),
        .execution_data = &self.context.value(),
        .wait_semas = {},
        .signal_semas = transfer_sema,
    });

    return true;
}

}  // namespace lr
