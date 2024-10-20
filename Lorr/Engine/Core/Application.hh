#pragma once

#include "ApplicationEvent.hh"
#include "EventManager.hh"

#include "Engine/Asset/Asset.hh"

#include "Engine/Graphics/Vulkan.hh"

#include "Engine/World/RenderPipeline.hh"
#include "Engine/World/World.hh"

#include "Engine/Window/Window.hh"

namespace lr {
struct ApplicationSurface {
    Window window = {};
    SwapChain swap_chain = {};
    ls::static_vector<ImageID, Device::Limits::FrameCount> images = {};
    ls::static_vector<ImageViewID, Device::Limits::FrameCount> image_views = {};
    b32 swap_chain_ok = false;
};

struct ApplicationInfo {
    ls::span<c8 *> args = {};
    WindowInfo window_info = {};
};

struct Application {
    // Prevent `Application` from being copied, `::get()` only has to return a reference
    Application() = default;
    Application(const Application &) = delete;
    Application(Application &&) = delete;
    Application &operator=(const Application &) = delete;
    Application &operator=(Application &&) = delete;

    static Application &get();

    EventManager<ApplicationEvent, ApplicationEventData> event_man = {};
    Device device = {};
    ApplicationSurface default_surface = {};
    AssetManager asset_man = {};

    RenderPipeline world_render_pipeline = {};
    World world = {};

    bool should_close = false;

    bool init(this Application &, const ApplicationInfo &info);
    void push_event(this Application &, ApplicationEvent event, const ApplicationEventData &data);

    void poll_events(this Application &);
    void run(this Application &);
    void shutdown(this Application &);

    virtual bool do_super_init(ls::span<c8 *> args) = 0;
    virtual bool do_prepare() = 0;
    virtual bool do_update(f32 delta_time) = 0;
    virtual void do_shutdown() = 0;

private:
    // TODO: Properly handle this when we need multiple windows/sc.
    bool create_surface(this Application &, ApplicationSurface &surface, std::optional<WindowInfo> window_info = std::nullopt);
};

}  // namespace lr
