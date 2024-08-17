#pragma once

#include "ApplicationEvent.hh"
#include "EventManager.hh"

#include "Engine/Asset/Asset.hh"

#include "Engine/Graphics/Device.hh"
#include "Engine/Graphics/Instance.hh"
#include "Engine/Graphics/SwapChain.hh"
#include "Engine/Graphics/Task/TaskGraph.hh"

#include "Engine/OS/Window.hh"

#include "Engine/World/Scene.hh"

namespace lr {
struct ApplicationSurface {
    Window window = {};
    SwapChain swap_chain = {};
    ls::static_vector<ImageID, Limits::FrameCount> images = {};
    ls::static_vector<ImageViewID, Limits::FrameCount> image_views = {};
    b32 swap_chain_ok = false;
};

struct ApplicationInfo {
    ls::span<c8 *> args = {};
    WindowInfo window_info = {};
};

struct Application {
    static Application &get();

    EventManager<ApplicationEvent, ApplicationEventData> event_manager = {};
    Instance instance = {};
    Device device = {};
    ApplicationSurface default_surface = {};
    AssetManager asset_man = {};

    TaskGraph task_graph = {};
    TaskImageID swap_chain_image_id = {};
    TaskImageID imgui_font_image_id = {};

    flecs::world ecs;
    std::vector<std::unique_ptr<Scene>> scenes = {};
    SceneID active_scene = SceneID::Invalid;

    bool init(this Application &, const ApplicationInfo &info);
    void push_event(this Application &, ApplicationEvent event, const ApplicationEventData &data);

    void poll_events(this Application &);
    void run(this Application &);

    template<typename T>
    std::pair<SceneID, T *> add_scene(this Application &self, std::string_view scene_name) {
        auto scene = std::make_unique<T>(std::string(scene_name), self.ecs);
        usize scene_id = self.scenes.size();
        T *scene_ptr = scene.get();
        self.scenes.push_back(std::move(scene));
        return { static_cast<SceneID>(scene_id), scene_ptr };
    }

    void set_active_scene(this Application &, SceneID scene_id);

    virtual bool do_prepare() = 0;
    virtual bool do_update(f32 delta_time) = 0;

private:
    // TODO: Properly handle this when we need multiple windows/sc.
    VKResult create_surface(this Application &, ApplicationSurface &surface, std::optional<WindowInfo> window_info = std::nullopt);
};

}  // namespace lr
