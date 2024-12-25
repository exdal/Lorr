#include "Application.hh"

#include <imgui.h>

namespace lr {
bool Application::init(this Application &self, const ApplicationInfo &info) {
    ZoneScoped;

    Logger::init("engine");

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

    if (!self.do_super_init(info.args)) {
        LOG_FATAL("Super init failed!");
        return false;
    }

    self.device.init(3);
    self.asset_man = AssetManager::create(&self.device);

    self.window = Window::create(info.window_info);
    auto window_size = self.window.get_size();
    auto surface = Surface::create(self.device, self.window.get_native_handle()).value();
    self.swap_chain = SwapChain::create(self.device, surface, { window_size.x, window_size.y }).value();

    auto world_result = World::create("default_world");
    if (!world_result.has_value()) {
        LOG_FATAL("Failed to create default world!");
        return false;
    }

    self.world = world_result.value();

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
    memory::ScopedStack stack;

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
            case ApplicationEvent::Drop: {
                break;
            }
            case ApplicationEvent::Quit: {
                self.should_close = true;
                break;
            }
            default:;
        }
    }
}

void Application::run(this Application &self) {
    ZoneScoped;

    // Renderer context
    auto &imgui = ImGui::GetIO();

    b32 swap_chain_ok = true;

    while (!self.should_close) {
        if (!swap_chain_ok) {
            auto surface = Surface::create(self.device, self.window.get_native_handle()).value();
            auto window_size = self.window.get_size();
            self.swap_chain = SwapChain::create(self.device, surface, { window_size.x, window_size.y }).value();

            swap_chain_ok = true;
        }

        auto swap_chain_attachment = self.device.new_frame(self.swap_chain);

        if (!self.world_renderer) {
            self.world_renderer = WorldRenderer::create(&self.device);
        }

        // Delta time
        f64 delta_time = self.world.delta_time();
        imgui.DeltaTime = static_cast<f32>(delta_time);

        // Update application
        auto extent = self.swap_chain.extent();
        imgui.DisplaySize = ImVec2(static_cast<f32>(extent.width), static_cast<f32>(extent.height));

        // WARN: Make sure to do all imgui settings BEFORE NewFrame!!!
        ImGui::NewFrame();
        self.world.begin_frame(self.world_renderer);
        self.do_update(delta_time);
        self.world.end_frame();
        self.should_close |= self.window.should_close();

        ImGui::Render();

        auto rendered_attachment = self.world_renderer.render(std::move(swap_chain_attachment));
        self.device.end_frame(std::move(rendered_attachment));

        self.window.poll();
        self.poll_events();
        self.should_close |= self.world.should_quit();

        FrameMark;
    }

    self.shutdown();
}

void Application::shutdown(this Application &self) {
    ZoneScoped;

    LOG_WARN("Shutting down application...");

    // self.device.wait();
    self.world.destroy();
    self.asset_man.destroy();
    // self.swap_chain.destroy();
    self.device.destroy();
    LOG_INFO("Complete!");
}

}  // namespace lr
