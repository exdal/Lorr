#include "RenderPipeline.hh"

#include "Engine/Core/Application.hh"

#include "Components.hh"
#include "RenderPipelineTasks.hh"

namespace lr {
bool RenderPipeline::init(this RenderPipeline &self, Device *device) {
    ZoneScoped;

    self.device = device;
    self.task_graph.init(TaskGraphInfo{ .device = device });

    if (!self.setup_imgui()) {
        LR_LOG_ERROR("Failed to setup ImGui pipeline!");
        return false;
    }

    if (!self.setup_resources()) {
        LR_LOG_ERROR("Failed to setup render pipeline resources!");
        return false;
    }

    if (!self.setup_passes()) {
        LR_LOG_ERROR("Failed to setup render passes!");
        return false;
    }

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

bool RenderPipeline::setup_imgui(this RenderPipeline &) {
    ZoneScoped;

    ImGui::CreateContext();
    auto &imgui = ImGui::GetIO();
    imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imgui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    imgui.IniFilename = nullptr;
    imgui.DisplayFramebufferScale = { 1.0f, 1.0f };
    imgui.BackendFlags = ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::StyleColorsDark();

    std::string roboto_path = (fs::current_path() / "resources/fonts/Roboto-Regular.ttf").string();
    std::string fa_regular_400_path = (fs::current_path() / "resources/fonts/fa-regular-400.ttf").string();
    std::string fa_solid_900_path = (fs::current_path() / "resources/fonts/fa-solid-900.ttf").string();

    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.MergeMode = true;
    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    imgui.Fonts->AddFontFromFileTTF(fa_regular_400_path.c_str(), 14.0f, &font_config, icons_ranges);
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);
    imgui.Fonts->Build();

    return true;
}

bool RenderPipeline::setup_resources(this RenderPipeline &self) {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &imgui = ImGui::GetIO();
    auto &transfer_queue = self.device->queue_at(CommandType::Transfer);
    auto cmd_list = transfer_queue.begin_command_list(0);

    // Load shaders
    asset_man.load_shader("shader://imgui_vs", { .entry_point = "vs_main", .path = "imgui.slang" });
    asset_man.load_shader("shader://imgui_fs", { .entry_point = "fs_main", .path = "imgui.slang" });
    asset_man.load_shader("shader://fullscreen_vs", { .entry_point = "vs_main", .path = "fullscreen.slang" });
    asset_man.load_shader("shader://fullscreen_fs", { .entry_point = "fs_main", .path = "fullscreen.slang" });
    asset_man.load_shader("shader://atmos.transmittance", { .entry_point = "cs_main", .path = "atmos/transmittance.slang" });
    asset_man.load_shader("shader://atmos.ms", { .entry_point = "cs_main", .path = "atmos/ms.slang" });
    asset_man.load_shader("shader://atmos.lut_vs", { .entry_point = "vs_main", .path = "atmos/lut.slang" });
    asset_man.load_shader("shader://atmos.lut_fs", { .entry_point = "fs_main", .path = "atmos/lut.slang" });
    asset_man.load_shader("shader://atmos.final_vs", { .entry_point = "vs_main", .path = "atmos/final.slang" });
    asset_man.load_shader("shader://atmos.final_fs", { .entry_point = "fs_main", .path = "atmos/final.slang" });
    asset_man.load_shader("shader://editor.grid_vs", { .entry_point = "vs_main", .path = "editor/grid.slang" });
    asset_man.load_shader("shader://editor.grid_fs", { .entry_point = "fs_main", .path = "editor/grid.slang" });

    // Load textures
    u8 *font_data = nullptr;  // imgui context frees this itself
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);

    // Images
    self.swap_chain_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = app.default_surface.images[0],
        .image_view_id = app.default_surface.image_views[0],
    });

    auto final_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::ColorAttachment,
        .extent = app.default_surface.swap_chain.extent,
        .debug_name = "Final Image",
    });
    auto final_image_view = app.device.create_image_view(ImageViewInfo{
        .image_id = final_image,
        .type = ImageViewType::View2D,
        .debug_name = "Final Image View",
    });
    self.final_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = final_image,
        .image_view_id = final_image_view,
    });

    auto imgui_font_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
        .format = Format::R8G8B8A8_UNORM,
        .type = ImageType::View2D,
        .extent = { static_cast<u32>(font_width), static_cast<u32>(font_height), 1u },
        .debug_name = "ImGui Font Atlas",
    });
    auto imgui_font_view = app.device.create_image_view({
        .image_id = imgui_font_image,
        .debug_name = "ImGui Font Atlas View",
    });
    app.device.set_image_data(imgui_font_image, font_data, ImageLayout::ColorReadOnly);
    self.imgui_font_image = self.task_graph.add_image({
        .image_id = imgui_font_image,
        .image_view_id = imgui_font_view,
        .layout = ImageLayout::ColorReadOnly,
        .access = PipelineAccess::FragmentShaderRead,
    });
    imgui.Fonts->SetTexID(reinterpret_cast<ImTextureID>(static_cast<iptr>(self.imgui_font_image)));

    auto transmittance_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::Storage | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::General,
        .extent = { 256, 64, 1 },
        .debug_name = "Sky Transmittance Image",
    });
    auto transmittance_view = app.device.create_image_view(ImageViewInfo{
        .image_id = transmittance_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky Transmittance View",
    });
    self.atmos_transmittance_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = transmittance_image,
        .image_view_id = transmittance_view,
        .layout = ImageLayout::ColorReadOnly,
        .access = PipelineAccess::FragmentShaderRead,
    });

    auto ms_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::Storage | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::General,
        .extent = { 256, 256, 1 },
        .debug_name = "Sky MS Image",
    });
    auto ms_view = app.device.create_image_view(ImageViewInfo{
        .image_id = ms_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky MS View",
    });
    self.atmos_ms_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = ms_image,
        .image_view_id = ms_view,
        .layout = ImageLayout::ColorReadOnly,
        .access = PipelineAccess::FragmentShaderRead,
    });

    auto sky_lut_image = app.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::ColorAttachment,
        .extent = { 400, 200, 1 },
        .debug_name = "Sky LUT",
    });
    auto sky_lut_view = app.device.create_image_view(ImageViewInfo{
        .image_id = sky_lut_image,
        .type = ImageViewType::View2D,
        .debug_name = "Sky LUT View",
    });
    self.atmos_sky_lut_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = sky_lut_image,
        .image_view_id = sky_lut_view,
    });

    // Load buffers
    self.dynamic_vertex_buffers.resize(self.device->frame_count);
    self.dynamic_index_buffers.resize(self.device->frame_count);
    self.world_data_buffers.resize(self.device->frame_count);
    self.world_camera_buffers.resize(self.device->frame_count);

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
    per_frame_buffer(self.world_data_buffers, "World Buffer", sizeof(GPUWorldData));
    for (usize i = 0; i < self.device->frame_count; i++) {
        self.cpu_upload_buffers.push_back(BufferID::Invalid);
    }

    transfer_queue.end_command_list(cmd_list);
    transfer_queue.submit(0, QueueSubmitInfo{ .self_wait = true });

    return true;
}

bool RenderPipeline::setup_passes(this RenderPipeline &self) {
    ZoneScoped;

    self.task_graph.add_task<SkyLUTTask>({
        .uses = {
            .attachment = self.atmos_sky_lut_image,
            .transmittance_lut = self.atmos_transmittance_image,
            .ms_lut = self.atmos_ms_image,
        },
    });
    self.task_graph.add_task<SkyFinalTask>({
        .uses = {
            .attachment = self.final_image,
            .sky_lut = self.atmos_sky_lut_image,
        },
    });

    self.task_graph.add_task<GridTask>({
        .uses = {
            .attachment = self.final_image,
        },
    });
    self.task_graph.add_task<ImGuiTask>({
        .uses = {
            .attachment = self.swap_chain_image,
            .font = self.imgui_font_image,
        },
    });
    self.task_graph.present(self.swap_chain_image);

    return true;
}

bool RenderPipeline::setup_persistent_images(this RenderPipeline &self) {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;
    auto &graphics_queue = self.device->queue_at(CommandType::Graphics);
    auto cmd_list = graphics_queue.begin_command_list(0);
    auto world_data_device_address = self.device->buffer_at(self.world_data_buffers[0]).device_address;

    auto sky_sampler_id = self.device->create_cached_sampler(SamplerInfo{
        .min_filter = Filtering::Linear,
        .mag_filter = Filtering::Linear,
        .mip_filter = Filtering::Linear,
        .address_u = TextureAddressMode::ClampToEdge,
        .address_v = TextureAddressMode::ClampToEdge,
        .min_lod = -1000,
        .max_lod = 1000,
    });

    // TODO: Delete pipeline IDs later
    {
        struct PushConstants {
            u64 world_ptr = 0;
            glm::vec2 image_size = {};
            ImageViewID image_view_id = ImageViewID::Invalid;
        } c = {};

        auto layout_id = static_cast<PipelineLayoutID>(sizeof(PushConstants) / sizeof(u32));
        ComputePipelineInfo pipeline_info = {
            .shader_id = asset_man.shader_at("shader://atmos.transmittance").value(),
            .layout_id = layout_id,
        };
        auto pipeline_id = self.device->create_compute_pipeline(pipeline_info);
        auto &task_image_info = self.task_graph.task_image_at(self.atmos_transmittance_image);
        auto &image_info = self.device->image_at(task_image_info.image_id);

        c.world_ptr = world_data_device_address;
        c.image_size = { image_info.extent.width, image_info.extent.height };
        c.image_view_id = task_image_info.image_view_id;

        cmd_list.set_pipeline(pipeline_id);
        cmd_list.set_descriptor_sets(layout_id, PipelineBindPoint::Compute, 0, self.device->descriptor_set);
        cmd_list.set_push_constants(layout_id, &c, sizeof(PushConstants), 0);
        cmd_list.dispatch(image_info.extent.width / 16 + 1, image_info.extent.height / 16 + 1, 1);
        cmd_list.image_transition(ImageBarrier{
            .src_access = PipelineAccess::All,
            .dst_access = PipelineAccess::FragmentShaderRead,
            .old_layout = ImageLayout::General,
            .new_layout = ImageLayout::ColorReadOnly,
            .image_id = task_image_info.image_id,
        });
    }

    {
        struct PushConstants {
            u64 world_ptr = 0;
            ImageViewID transmittance_image_id = {};
            SamplerID transmittance_sampler_id = {};
            glm::vec2 target_image_size = {};
            ImageViewID target_view_id = ImageViewID::Invalid;
        } c = {};

        auto layout_id = static_cast<PipelineLayoutID>(sizeof(PushConstants) / sizeof(u32));
        ComputePipelineInfo pipeline_info = {
            .shader_id = asset_man.shader_at("shader://atmos.ms").value(),
            .layout_id = layout_id,
        };
        auto pipeline_id = self.device->create_compute_pipeline(pipeline_info);
        auto &task_image_info = self.task_graph.task_image_at(self.atmos_ms_image);
        auto &image_info = self.device->image_at(task_image_info.image_id);
        auto &transmittance_task_image_info = self.task_graph.task_image_at(self.atmos_transmittance_image);

        c.world_ptr = world_data_device_address;
        c.transmittance_image_id = transmittance_task_image_info.image_view_id;
        c.transmittance_sampler_id = sky_sampler_id;
        c.target_image_size = { image_info.extent.width, image_info.extent.height };
        c.target_view_id = task_image_info.image_view_id;

        cmd_list.set_pipeline(pipeline_id);
        cmd_list.set_descriptor_sets(layout_id, PipelineBindPoint::Compute, 0, self.device->descriptor_set);
        cmd_list.set_push_constants(layout_id, &c, sizeof(PushConstants), 0);
        cmd_list.dispatch(image_info.extent.width / 16 + 1, image_info.extent.height / 16 + 1, 1);
        cmd_list.image_transition(ImageBarrier{
            .src_access = PipelineAccess::ComputeWrite,
            .dst_access = PipelineAccess::FragmentShaderRead,
            .old_layout = ImageLayout::General,
            .new_layout = ImageLayout::ColorReadOnly,
            .image_id = task_image_info.image_id,
        });
    }

    graphics_queue.end_command_list(cmd_list);
    graphics_queue.submit(0, QueueSubmitInfo{});

    return true;
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
    auto &ecs = app.ecs;
    auto &device = app.device;
    auto &transfer_queue = device.queue_at(CommandType::Transfer);
    auto cmd_list = transfer_queue.begin_command_list(frame_index);

    auto cameras = ecs.query<Prefab::PerspectiveCamera, Component::Transform, Component::Camera>();
    auto new_camera_buffer_size = cameras.count() * sizeof(GPUCameraData);
    auto upload_size = new_camera_buffer_size + sizeof(GPUWorldData);

    // IN ORDER:
    // Cameras
    auto &camera_buffer_id = self.world_camera_buffers[frame_index];
    update_buffer(camera_buffer_id, "World Camera Buffer", new_camera_buffer_size, BufferUsage::TransferDst);

    // Materials
    // TODO: Get this out of asset manager
    auto material_buffer_id = app.asset_man.material_buffer_id;

    // World data itself
    auto world_data_buffer_id = self.world_data_buffers[frame_index];
    self.world_data.cameras = device.buffer_at(camera_buffer_id).device_address;
    self.world_data.materials = device.buffer_at(material_buffer_id).device_address;

    // Upload stuff to its buffers
    auto &cpu_buffer_id = self.cpu_upload_buffers[frame_index];
    update_buffer(cpu_buffer_id, "Temp World Buffer", upload_size, BufferUsage::TransferSrc, true);
    auto cpu_buffer_data = device.buffer_host_data<u8>(cpu_buffer_id);
    auto cpu_buffer_offset = 0_sz;

    // Camera
    auto camera_ptr = reinterpret_cast<GPUCameraData *>(cpu_buffer_data + cpu_buffer_offset);
    BufferCopyRegion camera_copy_region = { .src_offset = 0, .dst_offset = 0, .size = new_camera_buffer_size };
    cameras.each([&](Prefab::PerspectiveCamera, Component::Transform &t, Component::Camera &c) {
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

    // World
    BufferCopyRegion world_copy_region = { .src_offset = cpu_buffer_offset, .dst_offset = 0, .size = sizeof(GPUWorldData) };
    std::memcpy(cpu_buffer_data + cpu_buffer_offset, &self.world_data, sizeof(GPUWorldData));
    cmd_list.copy_buffer_to_buffer(cpu_buffer_id, world_data_buffer_id, world_copy_region);

    transfer_queue.end_command_list(cmd_list);
    transfer_queue.submit(frame_index, QueueSubmitInfo{});

    if (self.device->frame_sema.counter == 0) {
        if (!self.setup_persistent_images()) {
            LR_LOG_ERROR("Failed to setup persistent images!");
            return false;
        }
    }

    return true;
}

bool RenderPipeline::render_into(this RenderPipeline &self, SwapChain &swap_chain, ls::span<ImageID> images, ls::span<ImageViewID> image_views) {
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
    self.device->staging_buffer_at(frame_index).reset();
    for (CommandQueue &cmd_queue : self.device->queues) {
        CommandAllocator &cmd_allocator = cmd_queue.command_allocator(frame_index);
        self.device->reset_command_allocator(cmd_allocator);
    }

    self.prepare(frame_index);
    u64 world_data_device_address = self.device->buffer_at(self.world_data_buffers[frame_index]).device_address;

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
        .execution_data = &world_data_device_address,
        .wait_semas = wait_semas,
        .signal_semas = signal_semas,
    });

    if (present_queue.present(swap_chain, present_sema, frame_index) == VKResult::OutOfDate) {
        return false;
    }

    return true;
}

void RenderPipeline::on_resize(this RenderPipeline &self, glm::uvec2 size) {
    ZoneScoped;

    // TODO: resize all images with ImageUsage::SwapChain with correct ratio

    self.task_graph.update_task_infos();
}

}  // namespace lr
