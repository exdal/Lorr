#include "RenderPipeline.hh"

#include "Engine/World/Components.hh"

#include "Engine/Core/Application.hh"

namespace lr {
bool RenderPipeline::init(this RenderPipeline &self, Device *device) {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    self.device = device;
    self.task_graph.init(TaskGraphInfo{ .device = device });

    ImGui::CreateContext();
    auto &imgui = ImGui::GetIO();
    imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imgui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    imgui.IniFilename = "editor_layout.ini";
    imgui.DisplayFramebufferScale = { 1.0f, 1.0f };
    imgui.BackendFlags = ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::StyleColorsDark();

    auto roboto_path = (fs::current_path() / "resources/fonts/Roboto-Regular.ttf").string();
    auto fa_solid_900_path = (fs::current_path() / "resources/fonts/fa-solid-900.ttf").string();

    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.GlyphMinAdvanceX = 16.0f;
    font_config.MergeMode = true;
    font_config.PixelSnapH = true;

    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    self.im_roboto_fa = imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);

    font_config.GlyphMinAdvanceX = 32.0f;
    font_config.MergeMode = false;
    self.im_fa_big = imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 32.0f, &font_config, icons_ranges);

    imgui.Fonts->Build();

    u8 *font_data = nullptr;  // imgui context frees this itself
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);

    ImageID imgui_font_image = self.device->create_image(ImageInfo{
        .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
        .format = Format::R8G8B8A8_UNORM,
        .type = ImageType::View2D,
        .extent = { static_cast<u32>(font_width), static_cast<u32>(font_height), 1u },
        .debug_name = "ImGui Font Atlas",
    });
    ImageViewID imgui_font_view = self.device->create_image_view({
        .image_id = imgui_font_image,
        .debug_name = "ImGui Font Atlas View",

    });
    self.device->set_image_data(imgui_font_image, font_data, ImageLayout::ColorReadOnly);
    imgui.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<iptr>(imgui_font_view)));

    imgui.FontDefault = self.im_roboto_fa;

    // Setup images
    self.swap_chain_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = app.default_surface.images[0],
        .image_view_id = app.default_surface.image_views[0],
    });

    auto geometry_depth_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::DepthStencilAttachment | ImageUsage::Sampled,
        .format = Format::D32_SFLOAT,
        .type = ImageType::View2D,
        .extent = app.default_surface.swap_chain.extent,
        .debug_name = "Geometry Depth Image",
    });
    auto geometry_depth_view = app.device.create_image_view(ImageViewInfo{
        .image_id = geometry_depth_image,
        .type = ImageViewType::View2D,
        .subresource_range = { .aspect_mask = ImageAspect::Depth },
        .debug_name = "Geometry Depth Image View",
    });
    self.geometry_depth_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = geometry_depth_image,
        .image_view_id = geometry_depth_view,
    });

    auto final_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .extent = app.default_surface.swap_chain.extent,
        .debug_name = "Final Image",
    });
    auto final_image_view = app.device.create_image_view(ImageViewInfo{
        .image_id = final_image,
        .type = ImageViewType::View2D,
        .debug_name = "Final Image View",
    });
    self.final_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = final_image,
        .image_view_id = final_image_view,
    });

    auto transmittance_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::Storage | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .extent = { 256, 64, 1 },
        .debug_name = "Sky Transmittance Image",
    });
    auto transmittance_view = app.device.create_image_view(ImageViewInfo{
        .image_id = transmittance_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky Transmittance View",
    });
    self.atmos_transmittance_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = transmittance_image,
        .image_view_id = transmittance_view,
    });

    auto ms_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::Storage | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .extent = { 256, 256, 1 },
        .debug_name = "Sky MS Image",
    });
    auto ms_view = app.device.create_image_view(ImageViewInfo{
        .image_id = ms_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky MS View",
    });
    self.atmos_ms_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = ms_image,
        .image_view_id = ms_view,
    });

    auto sky_lut_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .extent = { 400, 200, 1 },
        .debug_name = "Sky LUT",
    });
    auto sky_lut_view = app.device.create_image_view(ImageViewInfo{
        .image_id = sky_lut_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky LUT View",
    });
    self.atmos_sky_lut_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = sky_lut_image,
        .image_view_id = sky_lut_view,
    });

    auto sky_final_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .extent = app.default_surface.swap_chain.extent,
        .debug_name = "Sky Final",
    });
    auto sky_final_view = app.device.create_image_view(ImageViewInfo{
        .image_id = sky_final_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky Final View",
    });
    self.atmos_final_image = self.task_graph.add_image(TaskImageInfo{
        .image_id = sky_final_image,
        .image_view_id = sky_final_view,
    });

    SamplerID linear_sampler = self.device->create_sampler({
        .min_filter = Filtering::Linear,
        .mag_filter = Filtering::Linear,
        .mip_filter = Filtering::Linear,
        .address_u = TextureAddressMode::ClampToEdge,
        .address_v = TextureAddressMode::ClampToEdge,
    });
    self.tg_execution_data.linear_sampler = linear_sampler;

    SamplerID sky_sampler = self.device->create_sampler({
        .min_filter = Filtering::Linear,
        .mag_filter = Filtering::Linear,
        .mip_filter = Filtering::Linear,
        .address_u = TextureAddressMode::Repeat,
        .address_v = TextureAddressMode::ClampToEdge,
    });

    // Load buffers
    self.dynamic_vertex_buffers.resize(self.device->frame_count);
    self.dynamic_index_buffers.resize(self.device->frame_count);
    self.world_camera_buffers.resize(self.device->frame_count);
    self.world_model_buffers.resize(self.device->frame_count);
    self.world_data_buffers.resize(self.device->frame_count);

    auto per_frame_buffer = [&self](auto &vec, std::string_view name, usize size, BufferUsage additional_usage = BufferUsage::None) {
        for (auto &buffer_id : vec) {
            buffer_id = self.device->create_buffer(BufferInfo{
                .usage_flags = additional_usage | BufferUsage::TransferDst,
                .preference = MemoryPreference::Device,
                .data_size = size,
                .debug_name = name,
            });
        }
    };

    per_frame_buffer(self.dynamic_vertex_buffers, "Dynamic Vertex Buffer", sizeof(Vertex) * 1024 * 128, BufferUsage::Vertex);
    per_frame_buffer(self.dynamic_index_buffers, "Dynamic Index Buffer", sizeof(u32) * 1024 * 128, BufferUsage::Index);
    per_frame_buffer(self.world_camera_buffers, "Camera Buffer", sizeof(GPUCameraData) * 16);
    per_frame_buffer(self.world_model_buffers, "Model Buffer", sizeof(GPUModel) * 64);
    per_frame_buffer(self.world_data_buffers, "World Buffer", sizeof(GPUWorldData));
    for (usize i = 0; i < self.device->frame_count; i++) {
        self.cpu_upload_buffers.push_back(BufferID::Invalid);
    }

    // Setup tasks
    constexpr VertexAttribInfo IMGUI_VERTEX_LAYOUT[] = {
        { .format = Format::R32G32_SFLOAT, .location = 0, .offset = offsetof(ImDrawVert, pos) },
        { .format = Format::R32G32_SFLOAT, .location = 1, .offset = offsetof(ImDrawVert, uv) },
        { .format = Format::R8G8B8A8_UNORM, .location = 2, .offset = offsetof(ImDrawVert, col) },
    };

    auto sky_lut_program = asset_man.load_shader_program({ .path = "atmos/lut.slang" });
    auto sky_final_program = asset_man.load_shader_program({ .path = "atmos/final.slang" });
    auto geometry_program = asset_man.load_shader_program({ .path = "model.slang" });
    auto grid_program = asset_man.load_shader_program({ .path = "editor/grid.slang" });
    auto imgui_program = asset_man.load_shader_program({ .path = "imgui.slang" });

    auto sky_transmittance_program = asset_man.load_shader_program({ .path = "atmos/transmittance.slang" });
    auto sky_ms_program = asset_man.load_shader_program({ .path = "atmos/ms.slang" });

    auto sky_lut_pipeline = self.device->create_graphics_pipeline(  //
        GraphicsPipelineInfo()                                      //
            .set_program(std::get<GraphicsPipelineProgram>(sky_lut_program.value())));

    auto sky_final_pipeline = self.device->create_graphics_pipeline(  //
        GraphicsPipelineInfo()                                        //
            .set_program(std::get<GraphicsPipelineProgram>(sky_final_program.value())));

    auto geometry_pipeline = self.device->create_graphics_pipeline(  //
        GraphicsPipelineInfo()
            .set_program(std::get<GraphicsPipelineProgram>(geometry_program.value()))
            .set_vertex_layout(VERTEX_LAYOUT)
            .set_raster_state({
                .cull_mode = CullMode::Back,
            })
            .set_depth_stencil_state({
                .enable_depth_test = true,
                .enable_depth_write = true,
                .depth_compare_op = CompareOp::LessEqual,
            })
            .set_blend_attachments({ {
                .blend_enabled = true,
                .src_blend = BlendFactor::SrcAlpha,
                .dst_blend = BlendFactor::InvSrcAlpha,
                .blend_op = BlendOp::Add,
                .src_blend_alpha = BlendFactor::One,
                .dst_blend_alpha = BlendFactor::InvSrcAlpha,
                .blend_op_alpha = BlendOp::Add,
            } }));

    auto grid_pipeline = self.device->create_graphics_pipeline(  //
        GraphicsPipelineInfo()
            .set_program(std::get<GraphicsPipelineProgram>(grid_program.value()))
            .set_depth_stencil_state({
                .enable_depth_test = true,
                .enable_depth_write = true,
                .depth_compare_op = CompareOp::LessEqual,
            })
            .set_blend_attachments({ {
                .blend_enabled = true,
                .src_blend = BlendFactor::SrcAlpha,
                .dst_blend = BlendFactor::InvSrcAlpha,
                .blend_op = BlendOp::Add,
                .src_blend_alpha = BlendFactor::One,
                .dst_blend_alpha = BlendFactor::InvSrcAlpha,
                .blend_op_alpha = BlendOp::Add,
            } }));

    auto imgui_pipeline = self.device->create_graphics_pipeline(  //
        GraphicsPipelineInfo()
            .set_program(std::get<GraphicsPipelineProgram>(imgui_program.value()))
            .set_vertex_layout(IMGUI_VERTEX_LAYOUT)
            .set_blend_attachments({ {
                .blend_enabled = true,
                .src_blend = BlendFactor::One,
                .dst_blend = BlendFactor::InvSrcAlpha,
                .blend_op = BlendOp::Add,
                .src_blend_alpha = BlendFactor::One,
                .dst_blend_alpha = BlendFactor::InvSrcAlpha,
                .blend_op_alpha = BlendOp::Add,
            } }));

    self.task_graph.add_task({
        .name = "Sky LUT Task",
        .uses = {
            Preset::ColorAttachmentWrite(self.atmos_sky_lut_image),
            Preset::ColorReadOnly(self.atmos_transmittance_image),
            Preset::ColorReadOnly(self.atmos_ms_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &exec_data = tc.exec_data_as<TaskExecutionData>();

            RenderingAttachmentInfo color_attachment = {
                .image_view_id = sky_lut_view,
                .image_layout = ImageLayout::ColorAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { .color_clear = ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f) },
            };

            tc.cmd_list.begin_rendering({
                .render_area = tc.pass_rect(),
                .color_attachments = color_attachment,
            });
            tc.set_pipeline(sky_lut_pipeline);
            tc.cmd_list.set_viewport(0, tc.pass_viewport());
            tc.cmd_list.set_scissors(0, tc.pass_rect());

            struct {
                u64 world_ptr = exec_data.world_buffer_ptr;
                ImageViewID transmittance_image = transmittance_view;
                ImageViewID ms_lut = ms_view;
                SamplerID transmittance_sampler = linear_sampler;
            } c = {};
            tc.set_push_constants(c);
            tc.cmd_list.draw(3);
            tc.cmd_list.end_rendering();
        }
    });
    self.task_graph.add_task({
        .name = "Sky Final Task",
        .uses = {
            Preset::ColorAttachmentWrite(self.final_image),
            Preset::ColorReadOnly(self.atmos_sky_lut_image),
            Preset::ColorReadOnly(self.atmos_transmittance_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &exec_data = tc.exec_data_as<TaskExecutionData>();

            RenderingAttachmentInfo color_attachment = {
                .image_view_id = final_image_view,
                .image_layout = ImageLayout::ColorAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { .color_clear = ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f) },
            };

            tc.cmd_list.begin_rendering({
                .render_area = tc.pass_rect(),
                .color_attachments = color_attachment,
            });

            tc.set_pipeline(sky_final_pipeline);
            tc.cmd_list.set_viewport(0, tc.pass_viewport());
            tc.cmd_list.set_scissors(0, tc.pass_rect());

            struct {
                u64 world_ptr = exec_data.world_buffer_ptr;
                ImageViewID sky_lut = sky_lut_view;
                ImageViewID transmittance_image = transmittance_view;
                SamplerID sampler = sky_sampler;
            } c = {};
            tc.set_push_constants(c);
            tc.cmd_list.draw(3);
            tc.cmd_list.end_rendering();
        }
    });

    self.task_graph.add_task({
        .name = "Geometry Task",
        .uses = {
            Preset::ColorAttachmentWrite(self.final_image),
            Preset::DepthAttachmentWrite(self.geometry_depth_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &exec_data = tc.exec_data_as<TaskExecutionData>();

            RenderingAttachmentInfo color_attachment = {
                .image_view_id = final_image_view,
                .image_layout = ImageLayout::ColorAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { .color_clear = ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f) },
            };

            RenderingAttachmentInfo depth_attachment = {
                .image_view_id = geometry_depth_view,
                .image_layout = ImageLayout::DepthAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { .depth_clear = DepthClearValue(1.0f) },
            };

            tc.cmd_list.begin_rendering(RenderingBeginInfo{
                .render_area = tc.pass_rect(),
                .color_attachments = color_attachment,
                .depth_attachment = depth_attachment,
            });

            tc.set_pipeline(geometry_pipeline);
            tc.cmd_list.set_viewport(0, tc.pass_viewport());
            tc.cmd_list.set_scissors(0, tc.pass_rect());

            struct {
                u64 world_ptr = exec_data.world_buffer_ptr;
                GPUModelID model_id = GPUModelID::Invalid;
                GPUMaterialID material_id = GPUMaterialID::Invalid;
            } c = {};

            auto models = app.world.ecs.query<Component::RenderableModel>();
            models.each([&](Component::RenderableModel &m) {
                auto model = asset_man.model_at(m.model);

                tc.cmd_list.set_vertex_buffer(model->vertex_buffer.value());
                tc.cmd_list.set_index_buffer(model->index_buffer.value());

                for (auto &mesh : model->meshes) {
                    for (auto &primitive_index : mesh.primitive_indices) {
                        auto &primitive = model->primitives[primitive_index];

                        // c.material_id = static_cast<MaterialID>(primitive.material_index);
                        tc.set_push_constants(c);
                        tc.cmd_list.draw_indexed(primitive.index_count, primitive.index_offset, static_cast<i32>(primitive.vertex_offset));
                    }
                }
            });

            tc.cmd_list.end_rendering();
        }
    });
    self.task_graph.add_task({
        .name = "Grid Task",
        .uses = {
            Preset::ColorAttachmentWrite(self.final_image),
            Preset::DepthAttachmentWrite(self.geometry_depth_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &exec_data = tc.exec_data_as<TaskExecutionData>();

            RenderingAttachmentInfo color_attachment = {
                .image_view_id = final_image_view,
                .image_layout = ImageLayout::ColorAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { .color_clear = ColorClearValue(0.0f, 0.0f, 0.0f, 1.0f) },
            };

            RenderingAttachmentInfo depth_attachment = {
                .image_view_id = geometry_depth_view,
                .image_layout = ImageLayout::DepthAttachment,
                .load_op = AttachmentLoadOp::Clear,
                .store_op = AttachmentStoreOp::Store,
                .clear_value = { .depth_clear = DepthClearValue(1.0f) },
            };

            tc.cmd_list.begin_rendering(RenderingBeginInfo{
                .render_area = tc.pass_rect(),
                .color_attachments = color_attachment,
                .depth_attachment = depth_attachment,
            });

            tc.set_pipeline(grid_pipeline);
            tc.set_push_constants(exec_data.world_buffer_ptr);
            tc.cmd_list.draw(3);
            tc.cmd_list.end_rendering();
        }
    });

    self.task_graph.add_task({
        .name = "ImGui Task",
        .uses = {
            Preset::ColorAttachmentWrite(self.swap_chain_image),
        },
        .execute_cb = [&](TaskContext &tc) {
            auto &exec_data = tc.exec_data_as<TaskExecutionData>();
            auto &imgui = ImGui::GetIO();
            
            RenderingAttachmentInfo color_attachment = {
                .image_view_id = final_image_view,
                .image_layout = ImageLayout::ColorAttachment,
                .load_op = AttachmentLoadOp::Load,
                .store_op = AttachmentStoreOp::Store,
            };

            ImDrawData *draw_data = ImGui::GetDrawData();
            u64 vertex_size_bytes = draw_data->TotalVtxCount * sizeof(ImDrawVert);
            u64 index_size_bytes = draw_data->TotalIdxCount * sizeof(ImDrawIdx);
            if (!draw_data || vertex_size_bytes == 0) {
                return;
            }

        },
    });
    self.task_graph.present(self.swap_chain_image);

    return true;
}

void RenderPipeline::shutdown(this RenderPipeline &self) {
    ZoneScoped;

    self.device->delete_buffers(self.persistent_vertex_buffer);
    self.device->delete_buffers(self.persistent_index_buffer);
    self.device->delete_buffers(self.dynamic_vertex_buffers);
    self.device->delete_buffers(self.dynamic_index_buffers);
    self.device->delete_buffers(self.cpu_upload_buffers);
    self.device->delete_buffers(self.world_camera_buffers);
    self.device->delete_buffers(self.world_data_buffers);

    for (auto &v : self.task_graph.buffers) {
        self.device->delete_buffers(v.buffer_id);
    }

    for (auto &v : self.task_graph.images) {
        self.device->delete_image_views(v.image_view_id);
        self.device->delete_images(v.image_id);
    }
}

void RenderPipeline::update_world_data(this RenderPipeline &self) {
    ZoneScoped;

    auto &app = Application::get();
    auto &world = app.world;
    if (!world.active_scene.has_value()) {
        return;
    }

    auto &scene = world.scene_at(world.active_scene.value());
    auto directional_light_query =  //
        world.ecs
            .query_builder<Component::Transform, Component::DirectionalLight>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();

    directional_light_query.each([&self](flecs::entity, Component::Transform &t, Component::DirectionalLight &l) {
        auto rad = glm::radians(t.rotation);
        glm::vec3 direction = { glm::cos(rad.x) * glm::cos(rad.y), glm::sin(rad.y), glm::sin(rad.x) * glm::cos(rad.y) };
        self.world_data.sun.direction = glm::normalize(direction);
        self.world_data.sun.intensity = l.intensity;
    });
}

bool RenderPipeline::prepare(this RenderPipeline &self, usize frame_index) {
    ZoneScoped;

    auto update_buffer = [&self](auto &buffer_id, std::string_view name, usize target_size, BufferUsage buffer_usage, bool cpu = false) {
        bool refresh_buffer = (buffer_id == BufferID::Invalid) || self.device->buffer_at(buffer_id).data_size < target_size;
        if (refresh_buffer) {
            if (buffer_id != BufferID::Invalid) {
                self.device->delete_buffers(buffer_id);
            }

            buffer_id = self.device->create_buffer(BufferInfo{
                .usage_flags = buffer_usage,
                .flags = cpu ? MemoryFlag::HostSeqWrite : MemoryFlag::None,
                .preference = cpu ? MemoryPreference::Host : MemoryPreference::Device,
                .data_size = target_size,
                .debug_name = name,
            });
        }
    };

    auto &app = Application::get();
    auto &world = app.world;
    auto &device = app.device;
    auto &transfer_queue = device.queue_at(CommandType::Transfer);
    auto cmd_list = transfer_queue.begin_command_list(frame_index);

    // Update world data
    self.update_world_data();

    auto upload_size = sizeof(GPUWorldData);
    auto cameras = world.ecs.query<Component::Transform, Component::Camera>();
    auto new_camera_buffer_size = cameras.count() * sizeof(GPUCameraData);
    upload_size += new_camera_buffer_size;

    auto models = world.ecs.query<Component::Transform, Component::RenderableModel>();
    auto new_model_buffer_size = models.count() * sizeof(GPUModel);
    upload_size += new_model_buffer_size;

    // IN ORDER:
    // Cameras
    auto &camera_buffer_id = self.world_camera_buffers[frame_index];
    update_buffer(camera_buffer_id, "World Camera Buffer", new_camera_buffer_size, BufferUsage::TransferDst);
    // Models
    auto &model_buffer_id = self.world_model_buffers[frame_index];
    update_buffer(model_buffer_id, "World Model Buffer", new_model_buffer_size, BufferUsage::TransferDst);

    // Materials
    // TODO: Get this out of asset manager
    auto material_buffer_id = self.persistent_material_buffer;

    // World data itself
    auto world_data_buffer_id = self.world_data_buffers[frame_index];
    self.world_data.cameras = device.buffer_at(camera_buffer_id).device_address;
    self.world_data.materials = device.buffer_at(material_buffer_id).device_address;
    self.world_data.models = device.buffer_at(model_buffer_id).device_address;

    // Upload stuff to its buffers
    auto &cpu_buffer_id = self.cpu_upload_buffers[frame_index];
    update_buffer(cpu_buffer_id, "Temp World Buffer", upload_size, BufferUsage::TransferSrc, true);
    auto cpu_buffer_data = device.buffer_host_data<u8>(cpu_buffer_id);
    auto cpu_buffer_offset = 0_sz;

    // Camera
    auto camera_ptr = reinterpret_cast<GPUCameraData *>(cpu_buffer_data + cpu_buffer_offset);
    BufferCopyRegion camera_copy_region = { .src_offset = 0, .dst_offset = 0, .size = new_camera_buffer_size };
    cameras.each([&](Component::Transform &t, Component::Camera &c) {
        camera_ptr->projection_mat = c.projection;
        camera_ptr->view_mat = t.matrix;
        camera_ptr->projection_view_mat = glm::transpose(c.projection * t.matrix);
        camera_ptr->inv_view_mat = glm::inverse(glm::transpose(t.matrix));
        camera_ptr->inv_projection_view_mat = glm::inverse(glm::transpose(c.projection)) * camera_ptr->inv_view_mat;
        camera_ptr->position = t.position;

        camera_ptr++;
        cpu_buffer_offset += sizeof(GPUCameraData);
    });
    cmd_list.copy_buffer_to_buffer(cpu_buffer_id, camera_buffer_id, camera_copy_region);

    // Model
    auto model_ptr = reinterpret_cast<GPUModel *>(cpu_buffer_data + cpu_buffer_offset);
    BufferCopyRegion model_copy_region = { .src_offset = cpu_buffer_offset, .dst_offset = 0, .size = sizeof(GPUModel) };
    models.each([&](Component::Transform &t, Component::RenderableModel &) {
        model_ptr->transform_mat = t.matrix;

        model_ptr++;
        cpu_buffer_offset += sizeof(GPUModel);
    });
    cmd_list.copy_buffer_to_buffer(cpu_buffer_id, model_buffer_id, model_copy_region);

    // World
    BufferCopyRegion world_copy_region = { .src_offset = cpu_buffer_offset, .dst_offset = 0, .size = sizeof(GPUWorldData) };
    std::memcpy(cpu_buffer_data + cpu_buffer_offset, &self.world_data, sizeof(GPUWorldData));
    cmd_list.copy_buffer_to_buffer(cpu_buffer_id, world_data_buffer_id, world_copy_region);

    transfer_queue.end_command_list(cmd_list);
    transfer_queue.submit(frame_index, QueueSubmitInfo{});

    // if (self.device->frame_sema.counter == 0) {
    //     if (!self.setup_persistent_images()) {
    //         LOG_ERROR("Failed to setup persistent images!");
    //         return false;
    //     }
    // }

    return true;
}

bool RenderPipeline::render_into(
    this RenderPipeline &self, SwapChain &swap_chain, ls::span<ImageID> images, ls::span<ImageViewID> image_views) {
    ZoneScoped;

    auto &frame_sema = self.device->frame_sema;
    auto &present_queue = self.device->queue_at(CommandType::Graphics);
    usize sema_index = self.device->new_frame();
    auto [acquire_sema, present_sema] = swap_chain.binary_semas(sema_index);
    auto [frame_index, acq_result] = self.device->acquire_next_image(swap_chain, acquire_sema);
    if (acq_result != VKResult::Success) {
        return false;
    }

    // Reset frame resources
    self.device->staging_buffers.at(frame_index).reset();
    for (CommandQueue &cmd_queue : self.device->queues) {
        CommandAllocator &cmd_allocator = cmd_queue.command_allocator(frame_index);
        self.device->reset_command_allocator(cmd_allocator);
    }

    self.prepare(frame_index);
    u64 world_data_device_address = self.device->buffer_at(self.world_data_buffers[frame_index]).device_address;
    self.tg_execution_data.world_buffer_ptr = world_data_device_address;

    self.task_graph.set_image(
        self.swap_chain_image,
        {
            .image_id = images[frame_index],
            .image_view_id = image_views[frame_index],
        });

    SemaphoreSubmitInfo wait_semas[] = {
        { acquire_sema, 0, PipelineStage::TopOfPipe },
    };
    SemaphoreSubmitInfo signal_semas[] = {
        { present_sema, 0, PipelineStage::BottomOfPipe },
        { frame_sema, frame_sema.counter + 1, PipelineStage::AllCommands },
    };

    self.task_graph.execute({
        .frame_index = frame_index,
        .execution_data = &self.tg_execution_data,
        .wait_semas = wait_semas,
        .signal_semas = signal_semas,
    });

    if (present_queue.present(swap_chain, present_sema, frame_index) == VKResult::OutOfDate) {
        return false;
    }

    return true;
}

}  // namespace lr
