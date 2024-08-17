#include "Application.hh"

#include "Engine/Memory/Stack.hh"

namespace lr {
bool Application::init(this Application &self, const ApplicationInfo &info) {
    ZoneScoped;

    Log::init(static_cast<i32>(info.args.size()), info.args.data());

    if (!self.instance.init({ .engine_name = "Lorr" })) {
        return false;
    }

    if (!self.device.init({ .instance = &self.instance.m_handle, .frame_count = 3 })) {
        return false;
    }

    if (!self.create_surface(self.default_surface, info.window_info)) {
        return false;
    }

    if (!self.asset_man.init(&self.device)) {
        return false;
    }

    self.task_graph.init({ .device = &self.device });
    self.swap_chain_image_id = self.task_graph.add_image({
        .image_id = self.default_surface.images[0],
        .image_view_id = self.default_surface.image_views[0],
    });

    /// IMGUI ///
    ImGui::CreateContext();
    auto &imgui = ImGui::GetIO();
    imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imgui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    imgui.IniFilename = nullptr;
    imgui.DisplayFramebufferScale = { 1.0f, 1.0f };
    imgui.BackendFlags = ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::StyleColorsDark();

    // TODO: Move this to asset manager
    //

    auto roboto_path = fs::current_path() / "resources/fonts/Roboto-Regular.ttf";
    auto fa_regular_400_path = fs::current_path() / "resources/fonts/fa-regular-400.ttf";
    auto fa_solid_900_path = fs::current_path() / "resources/fonts/fa-solid-900.ttf";

    ImWchar icons_ranges[] = { 0xf000, 0xf8ff, 0 };
    ImFontConfig font_config;
    font_config.MergeMode = true;
    imgui.Fonts->AddFontFromFileTTF(roboto_path.c_str(), 16.0f, nullptr);
    imgui.Fonts->AddFontFromFileTTF(fa_regular_400_path.c_str(), 14.0f, &font_config, icons_ranges);
    imgui.Fonts->AddFontFromFileTTF(fa_solid_900_path.c_str(), 14.0f, &font_config, icons_ranges);
    imgui.Fonts->Build();

    u8 *font_data = nullptr;  // imgui context frees this itself
    i32 font_width, font_height;
    imgui.Fonts->GetTexDataAsRGBA32(&font_data, &font_width, &font_height);

    ImageID imgui_font_id = self.device.create_image({
        .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
        .format = Format::R8G8B8A8_UNORM,
        .type = ImageType::View2D,
        .extent = { static_cast<u32>(font_width), static_cast<u32>(font_height), 1u },
        .debug_name = "ImGui Font Atlas",
    });
    self.device.set_image_data(imgui_font_id, font_data, ImageLayout::ColorReadOnly);

    ImageViewID imgui_font_view_id = self.device.create_image_view({
        .image_id = imgui_font_id,
        .usage_flags = ImageUsage::Sampled | ImageUsage::TransferDst,
        .debug_name = "ImGui Font Atlas View",
    });

    self.imgui_font_image_id = self.task_graph.add_image({
        .image_id = imgui_font_id,
        .image_view_id = imgui_font_view_id,
        .layout = ImageLayout::ColorReadOnly,
        .access = PipelineAccess::FragmentShaderRead,
    });
    imgui.Fonts->SetTexID(nullptr);

    if (!self.do_prepare()) {
        LR_LOG_FATAL("Failed to initialize application!");
        return false;
    }

    self.run();
    return true;
}

void Application::push_event(this Application &self, ApplicationEvent event, const ApplicationEventData &data) {
    ZoneScoped;

    self.event_manager.push(event, data);
}

void Application::poll_events(this Application &self) {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    while (self.event_manager.peek()) {
        ApplicationEventData e = {};
        switch (self.event_manager.dispatch(e)) {
            case ApplicationEvent::WindowResize: {
                break;
            }
            case ApplicationEvent::MousePosition: {
                auto &pos = e.mouse_pos;
                imgui.AddMousePosEvent(pos.x, pos.y);
                break;
            }
            case ApplicationEvent::MouseState: {
                imgui.AddMouseButtonEvent(e.mouse_key, e.mouse_key_state == KeyState::Down);
                break;
            }
            case ApplicationEvent::MouseScroll: {
                auto &offset = e.mouse_scroll_offset;
                imgui.AddMouseWheelEvent(offset.x, offset.y);
                break;
            }
            case ApplicationEvent::KeyboardState: {
                break;
            }
            case ApplicationEvent::InputChar: {
                imgui.AddInputCharacterUTF16(e.input_char);
                break;
            }
            default: {
                break;
            }
        }
    }
}

void Application::run(this Application &self) {
    ZoneScoped;

    f64 last_time = 0.0f;

    // Surface info
    auto &window = self.default_surface.window;
    auto &swap_chain = self.default_surface.swap_chain;
    auto &images = self.default_surface.images;
    auto &image_views = self.default_surface.image_views;

    // Renderer context
    auto &frame_sema = self.device.frame_sema;
    auto &imgui = ImGui::GetIO();
    auto &present_queue = self.device.queue_at(CommandType::Graphics);

    while (!window.should_close()) {
        if (!self.default_surface.swap_chain_ok) {
            self.create_surface(self.default_surface);
            self.default_surface.swap_chain_ok = true;
        }

        // Delta time
        f64 cur_time = glfwGetTime();
        f32 delta_time = static_cast<f32>(cur_time - last_time);
        imgui.DeltaTime = delta_time;
        last_time = cur_time;

        // Begin frame
        usize sema_index = self.device.new_frame();
        auto [acquire_sema, present_sema] = swap_chain.binary_semas(sema_index);
        auto [frame_index, acq_result] = self.device.acquire_next_image(swap_chain, acquire_sema);
        if (acq_result != VKResult::Success) {
            self.default_surface.swap_chain_ok = false;
            continue;
        }

        // Reset frame resources
        self.device.staging_buffer_at(frame_index).reset();
        for (CommandQueue &cmd_queue : self.device.queues) {
            CommandAllocator &cmd_allocator = cmd_queue.command_allocator(frame_index);
            self.device.reset_command_allocator(cmd_allocator);
        }

        ImageID image_id = images[frame_index];
        ImageViewID image_view_id = image_views[frame_index];
        self.task_graph.set_image(self.swap_chain_image_id, { .image_id = image_id, .image_view_id = image_view_id });

        // Update application
        auto &extent = self.default_surface.swap_chain.extent;
        imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

        // NOTE: Make sure to do all imgui settings BEFORE NewFrame!!!
        ImGui::NewFrame();
        self.do_update(delta_time);
        ImGui::Render();

        // Render frame
        SemaphoreSubmitInfo wait_semas[] = {
            { acquire_sema, 0, PipelineStage::TopOfPipe },
        };
        SemaphoreSubmitInfo signal_semas[] = {
            { present_sema, 0, PipelineStage::BottomOfPipe },
            { frame_sema, frame_sema.counter + 1, PipelineStage::AllCommands },
        };
        self.task_graph.execute({ .frame_index = frame_index, .wait_semas = wait_semas, .signal_semas = signal_semas });

        if (present_queue.present(swap_chain, present_sema, frame_index) == VKResult::OutOfDate) {
            self.default_surface.swap_chain_ok = false;
        }

        self.device.end_frame();
        window.poll();
        self.poll_events();

        FrameMark;
    }
}

void Application::set_active_scene(this Application &self, SceneID scene_id) {
    usize scene_idx = static_cast<usize>(scene_id);
    if (scene_id != SceneID::Invalid && scene_idx < self.scenes.size()) {
        self.active_scene = scene_id;
    }
}

VKResult Application::create_surface(this Application &self, ApplicationSurface &surface, std::optional<WindowInfo> window_info) {
    ZoneScoped;
    memory::ScopedStack stack;

    if (window_info != std::nullopt) {
        surface.window.init(*window_info);
    }

    if (surface.swap_chain) {
        self.device.delete_swap_chains(self.default_surface.swap_chain);
        self.device.delete_image_views(self.default_surface.image_views);
        self.device.delete_images(self.default_surface.images);
    }

    surface.images.resize(self.device.frame_count);
    surface.image_views.resize(self.device.frame_count);

    SwapChainInfo swap_chain_info = {
        .surface = surface.window.get_surface(self.instance.m_handle),
        .extent = { surface.window.width, surface.window.height },
    };

    if (auto result = self.device.create_swap_chain(surface.swap_chain, swap_chain_info); !result) {
        return result;
    }
    self.device.get_swapchain_images(surface.swap_chain, surface.images);
    for (usize i = 0; i < self.device.frame_count; i++) {
        // Image
        self.device.set_object_name(self.device.image_at(surface.images[i]), stack.format("SwapChain Image {}", i));

        // ImageView
        ImageViewInfo image_view_info = {
            .image_id = surface.images[i],
            .usage_flags = ImageUsage::TransferDst,
            .debug_name = stack.format("SwapChain ImageView {}", i),
        };
        auto [image_view_id, image_view_result] = self.device.create_image_view(image_view_info);
        if (!image_view_result) {
            LR_LOG_ERROR("Failed to create ImageView({}) for SwapChain!", i);
            return {};
        }

        surface.image_views[i] = image_view_id;
    }

    self.task_graph.update_task_infos();

    return VKResult::Success;
}

}  // namespace lr
