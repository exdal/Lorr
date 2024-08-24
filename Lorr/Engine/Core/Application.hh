#pragma once

#include "ApplicationEvent.hh"
#include "EventManager.hh"

#include "Engine/Asset/Asset.hh"

#include "Engine/Graphics/Device.hh"
#include "Engine/Graphics/Instance.hh"
#include "Engine/Graphics/SwapChain.hh"

#include "Engine/OS/Window.hh"

#include "Engine/World/RenderPipeline.hh"
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

    flecs::world ecs;
    std::vector<std::unique_ptr<Scene>> scenes = {};
    ls::option<SceneID> active_scene = ls::nullopt;

    RenderPipeline main_render_pipeline = {};

    bool init(this Application &, const ApplicationInfo &info);
    void setup_world(this Application &);
    void push_event(this Application &, ApplicationEvent event, const ApplicationEventData &data);

    void poll_events(this Application &);
    void run(this Application &);
    void shutdown(this Application &, bool hard);

    template<typename T>
    std::pair<SceneID, T *> add_scene(this Application &self, std::string_view scene_name) {
        auto scene = std::make_unique<T>(std::string(scene_name), self.ecs);
        usize scene_id = self.scenes.size();
        T *scene_ptr = scene.get();
        self.scenes.push_back(std::move(scene));
        return { static_cast<SceneID>(scene_id), scene_ptr };
    }
    void set_active_scene(this Application &, SceneID scene_id);
    Scene &scene_at(this Application &self, SceneID scene_id) { return *self.scenes[static_cast<usize>(scene_id)]; }

    virtual bool do_prepare() = 0;
    virtual bool do_update(f32 delta_time) = 0;
    virtual void do_shutdown() = 0;

private:
    // TODO: Properly handle this when we need multiple windows/sc.
    VKResult create_surface(this Application &, ApplicationSurface &surface, std::optional<WindowInfo> window_info = std::nullopt);
};

}  // namespace lr
