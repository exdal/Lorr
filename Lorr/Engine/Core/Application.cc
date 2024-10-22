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

    auto device = Device::create(3);
    if (!device.has_value()) {
        LOG_ERROR("Failed to create application! Device failed.");
        return false;
    }
    self.device = device.value();

    self.window = Window::create(info.window_info);

    if (!self.asset_man.init(self.device)) {
        return false;
    }

    if (!self.world.init()) {
        return false;
    }

    if (!self.world_render_pipeline.init(self.device)) {
        return false;
    }

    ImGui::CreateContext();
    auto &imgui = ImGui::GetIO();
    imgui.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    imgui.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    imgui.IniFilename = "editor_layout.ini";
    imgui.DisplayFramebufferScale = { 1.0f, 1.0f };
    imgui.BackendFlags = ImGuiBackendFlags_RendererHasVtxOffset;
    ImGui::StyleColorsDark();

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

    // Renderer context
    auto &imgui = ImGui::GetIO();
    std::vector<ImageID> images = {};
    std::vector<ImageViewID> image_views = {};
    b32 swap_chain_ok = false;

    while (true) {
        bool should_close = false;
        should_close |= self.window.should_close();
        should_close |= self.world.ecs.should_quit();
        should_close |= self.should_close;
        if (should_close) {
            break;
        }

        if (!swap_chain_ok) {
            auto surface = Surface::create(self.device, self.window.get_native_handle()).value();
            auto window_size = self.window.get_size();
            self.swap_chain = SwapChain::create(self.device, surface, { window_size.x, window_size.y }).value();
            std::tie(images, image_views) = self.swap_chain.get_images();

            swap_chain_ok = true;
        }

        // Delta time
        f64 delta_time = self.world.ecs.delta_time();
        imgui.DeltaTime = delta_time;

        // Update application
        auto extent = self.swap_chain.extent();
        imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

        self.world.begin_frame();
        // WARN: Make sure to do all imgui settings BEFORE NewFrame!!!
        // ImGui::NewFrame();
        // self.do_update(delta_time);
        // ImGui::Render();
        self.world.end_frame();

        // Render frame
        self.world_render_pipeline.render_into(self.swap_chain, images, image_views);

        self.device.end_frame();
        self.window.poll();
        self.poll_events();

        FrameMark;
    }

    self.shutdown();
}

void Application::shutdown(this Application &self) {
    ZoneScoped;

    LOG_WARN("Shutting down application...");

    self.device.wait();
    self.world.shutdown();
#ifdef LS_DEBUG
    self.asset_man.shutdown(true);
#else
    self.asset_man.shutdown(false);
#endif
    self.swap_chain.destroy();
    self.world_render_pipeline.shutdown();
#ifdef LS_DEBUG
    self.device.destroy();
#else
    self.device.destroy();
#endif
    LOG_INFO("Complete!");
}

}  // namespace lr
