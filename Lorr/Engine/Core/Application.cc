#include "Application.hh"

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

    if (!self.do_super_init(info.args)) {
        LOG_FATAL("Super init failed!");
        return false;
    }

    self.device.init(3).value();
    self.asset_man = AssetManager::create(&self.device);

    self.window = Window::create(info.window_info);
    auto window_size = self.window.get_size();
    auto surface = self.window.get_surface(self.device.get_instance());
    self.swapchain = SwapChain::create(self.device, surface, { window_size.x, window_size.y }).value();
    self.imgui_renderer.init(&self.device);

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

void Application::run(this Application &self) {
    ZoneScoped;

    WindowCallbacks window_callbacks = {};
    window_callbacks.user_data = &self;
    window_callbacks.on_close = [](void *user_data) {
        auto app = static_cast<Application *>(user_data);
        app->should_close = true;
    };
    window_callbacks.on_mouse_pos = [](void *, glm::vec2 position, glm::vec2) {
        auto &imgui = ImGui::GetIO();
        imgui.AddMousePosEvent(position.x, position.y);
    };
    window_callbacks.on_mouse_button = [](void *, Key mouse_key, KeyState state) {
        auto &imgui = ImGui::GetIO();
        imgui.AddMouseButtonEvent(static_cast<i32>(mouse_key), state == KeyState::Down);
    };
    window_callbacks.on_mouse_scroll = [](void *, glm::vec2 offset) {
        auto &imgui = ImGui::GetIO();
        imgui.AddMouseWheelEvent(offset.x, offset.y);
    };
    window_callbacks.on_key = [](void *, Key key, KeyState state, KeyMod) {
        auto &imgui = ImGui::GetIO();
        imgui.AddKeyEvent(static_cast<ImGuiKey>(key), state == KeyState::Down);
    };
    window_callbacks.on_text_input = [](void *, c8 *text) {
        auto &imgui = ImGui::GetIO();
        imgui.AddInputCharactersUTF8(text);
    };

    b32 swapchain_ok = true;
    while (!self.should_close) {
        if (!swapchain_ok) {
            auto window_size = self.window.get_size();
            auto surface = self.window.get_surface(self.device.get_instance());
            self.swapchain = SwapChain::create(self.device, surface, { window_size.x, window_size.y }).value();

            swapchain_ok = true;
        }

        auto swapchain_attachment = self.device.new_frame(self.swapchain);
        swapchain_attachment = vuk::clear_image(std::move(swapchain_attachment), vuk::Black<f32>);

        if (!self.world_renderer) {
            self.world_renderer = WorldRenderer::create(&self.device);
        }

        // Delta time
        self.world.progress();
        self.window.poll(window_callbacks);

        f64 delta_time = self.world.delta_time();
        self.imgui_renderer.begin_frame(delta_time, self.swapchain.extent());

        self.do_update(delta_time);
        if (self.world.update_scene_data(self.world_renderer)) {
            self.world_renderer_image_index = self.imgui_renderer.add_image(self.world_renderer.render(swapchain_attachment));
        }

        swapchain_attachment = self.imgui_renderer.end_frame(std::move(swapchain_attachment));
        self.device.end_frame(std::move(swapchain_attachment));

        self.should_close |= self.world.should_quit();

        FrameMark;
    }

    self.shutdown();
}

void Application::shutdown(this Application &self) {
    ZoneScoped;

    LOG_WARN("Shutting down application...");

    self.device.wait();
    self.swapchain.destroy();
    self.world.destroy();
    self.asset_man.destroy();
    self.device.destroy();
    LOG_INFO("Complete!");
}

}  // namespace lr
