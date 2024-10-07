#include "Application.hh"

#include "Engine/Memory/Stack.hh"

namespace lr {
bool Application::init(this Application &self, const ApplicationInfo &info) {
    ZoneScoped;

    Logger::init("engine");

    if (!self.do_super_init(info.args)) {
        LOG_FATAL("Super init failed!");
        return false;
    }

    if (!self.instance.init({ .engine_name = "Lorr" })) {
        return false;
    }

    if (!self.device.init({ .instance = &self.instance.handle, .frame_count = 3 })) {
        return false;
    }

    if (!self.create_surface(self.default_surface, info.window_info)) {
        return false;
    }

    if (!self.asset_man.init(&self.device)) {
        return false;
    }

    if (!self.world.init()) {
        return false;
    }

    if (!self.world_render_pipeline.init(&self.device)) {
        return false;
    }

    auto &imgui = ImGui::GetIO();
    imgui.KeyMap[ImGuiKey_W] = LR_KEY_W;
    imgui.KeyMap[ImGuiKey_A] = LR_KEY_A;
    imgui.KeyMap[ImGuiKey_S] = LR_KEY_S;
    imgui.KeyMap[ImGuiKey_D] = LR_KEY_D;
    imgui.KeyMap[ImGuiKey_Enter] = LR_KEY_ENTER;
    imgui.KeyMap[ImGuiKey_Backspace] = LR_KEY_BACKSPACE;
    imgui.KeyMap[ImGuiKey_LeftArrow] = LR_KEY_LEFT;
    imgui.KeyMap[ImGuiKey_RightArrow] = LR_KEY_RIGHT;

    if (!self.do_prepare()) {
        LOG_FATAL("Failed to initialize application!");
        return false;
    }

    self.run();
    return true;
}

void Application::push_event(this Application &self, ApplicationEvent event, const ApplicationEventData &data) {
    ZoneScoped;

    self.event_man.push(event, data);
}

void Application::poll_events(this Application &self) {
    ZoneScoped;

    auto &imgui = ImGui::GetIO();
    while (self.event_man.peek()) {
        ApplicationEventData e = {};
        switch (self.event_man.dispatch(e)) {
            case ApplicationEvent::WindowResize: {
                self.world_render_pipeline.on_resize(e.size);
                break;
            }
            case ApplicationEvent::MousePosition: {
                auto &pos = e.position;
                imgui.AddMousePosEvent(pos.x, pos.y);
                break;
            }
            case ApplicationEvent::MouseState: {
                bool down = e.key_state == KeyState::Down || e.key_state == KeyState::Repeat;
                imgui.AddMouseButtonEvent(e.key, down);
                break;
            }
            case ApplicationEvent::MouseScroll: {
                auto &offset = e.position;
                imgui.AddMouseWheelEvent(offset.x, offset.y);
                break;
            }
            case ApplicationEvent::KeyboardState: {
                bool down = e.key_state == KeyState::Down || e.key_state == KeyState::Repeat;
                imgui.AddKeyEvent(static_cast<ImGuiKey>(e.key), down);
                break;
            }
            case ApplicationEvent::InputChar: {
                imgui.AddInputCharacterUTF16(e.input_char);
                break;
            }
            case ApplicationEvent::Quit: {
                self.should_close = true;
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
    auto &imgui = ImGui::GetIO();

    while (true) {
        bool should_close = false;
        should_close |= window.should_close();
        should_close |= self.world.ecs.should_quit();
        should_close |= self.should_close;
        if (should_close) {
            break;
        }

        if (!self.default_surface.swap_chain_ok) {
            self.create_surface(self.default_surface);
            self.default_surface.swap_chain_ok = true;
        }

        // Delta time
        f64 cur_time = glfwGetTime();
        f32 delta_time = static_cast<f32>(cur_time - last_time);
        imgui.DeltaTime = delta_time;
        last_time = cur_time;

        // Update application
        auto &extent = self.default_surface.swap_chain.extent;
        imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

        self.world.begin_frame();
        // WARN: Make sure to do all imgui settings BEFORE NewFrame!!!
        ImGui::NewFrame();
        self.do_update(delta_time);
        ImGui::Render();
        self.world.end_frame();

        // Render frame
        self.world_render_pipeline.render_into(swap_chain, images, image_views);

        self.device.end_frame();
        window.poll();
        self.poll_events();

        FrameMark;
    }

    self.shutdown();
}

void Application::shutdown(this Application &self) {
    ZoneScoped;

    LOG_WARN("Shutting down application...");

    self.device.wait_for_work();
    self.world.shutdown();
#ifdef LS_DEBUG
    self.asset_man.shutdown(true);
#else
    self.asset_man.shutdown(false);
#endif
    self.device.delete_swap_chains(self.default_surface.swap_chain);
    self.world_render_pipeline.shutdown();
#ifdef LS_DEBUG
    self.device.shutdown(true);
#else
    self.device.shutdown(false);
#endif
    LOG_INFO("Complete!");
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
        .surface = surface.window.get_surface(self.instance.handle),
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
            .debug_name = stack.format("SwapChain ImageView {}", i),
        };
        auto [image_view_id, image_view_result] = self.device.create_image_view(image_view_info);
        if (!image_view_result) {
            LOG_ERROR("Failed to create ImageView({}) for SwapChain!", i);
            return {};
        }

        surface.image_views[i] = image_view_id;
    }

    return VKResult::Success;
}

}  // namespace lr
