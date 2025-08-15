#include "Runtime/RuntimeModule.hh"

#include "Engine/Asset/Asset.hh"
#include "Engine/Core/App.hh"
#include "Engine/Graphics/ImGuiRenderer.hh"
#include "Engine/Graphics/VulkanDevice.hh"
#include "Engine/Scene/ECSModule/Core.hh"
#include "Engine/Window/Window.hh"

auto RuntimeModule::init(this RuntimeModule &self) -> bool {
    LOG_TRACE("Actvie world: {}", self.world_path);

    auto &asset_man = lr::App::mod<lr::AssetManager>();
    asset_man.import_project(self.world_path);

    return true;
}

auto RuntimeModule::update(this RuntimeModule &self, f64 delta_time) -> void {
    auto &window = lr::App::mod<lr::Window>();
    auto &device = lr::App::mod<lr::Device>();
    auto &imgui_renderer = lr::App::mod<lr::ImGuiRenderer>();
    auto &asset_man = lr::App::mod<lr::AssetManager>();
    auto &scene_renderer = lr::App::mod<lr::SceneRenderer>();

    auto swapchain_attachment = device.new_frame(window.swap_chain.value());
    swapchain_attachment = vuk::clear_image(std::move(swapchain_attachment), vuk::Black<f32>);
    imgui_renderer.begin_frame(delta_time, swapchain_attachment->extent);

    if (self.active_scene_uuid) {
        auto *active_scene = asset_man.get_scene(self.active_scene_uuid);
        auto camera_query = active_scene //
                                ->get_world()
                                .query_builder<lr::ECS::Camera, lr::ECS::ActiveCamera>()
                                .build();

        camera_query.each([&window](flecs::entity, lr::ECS::Camera &c, lr::ECS::ActiveCamera) {
            c.resolution = glm::vec2(window.width, window.height);
            c.aspect_ratio = c.resolution.x / c.resolution.y;
        });

        active_scene->tick(static_cast<f32>(delta_time));

        auto prepared_frame = active_scene->prepare_frame(scene_renderer);
        auto scene_render_info = lr::SceneRenderInfo{
            .delta_time = static_cast<f32>(delta_time),
            .cull_flags = active_scene->get_cull_flags(),
        };
        swapchain_attachment = scene_renderer.render(std::move(swapchain_attachment), scene_render_info, prepared_frame);
    }

    if (ImGui::Begin("Runtime")) {
        const auto &registry = asset_man.get_registry();
        for (const auto &[asset_uuid, asset] : registry) {
            if (asset.type != lr::AssetType::Scene) {
                continue;
            }

            if (ImGui::Button(asset.path.c_str())) {
                if (self.active_scene_uuid) {
                    asset_man.unload_scene(self.active_scene_uuid);
                }

                if (asset_man.load_scene(asset_uuid)) {
                    self.active_scene_uuid = asset_uuid;
                }
            }
        }
    }
    ImGui::End();

    swapchain_attachment = imgui_renderer.end_frame(std::move(swapchain_attachment));
    device.end_frame(std::move(swapchain_attachment));
}

auto RuntimeModule::destroy(this RuntimeModule &self) -> void {
    auto &asset_man = lr::App::mod<lr::AssetManager>();

    if (self.active_scene_uuid) {
        asset_man.unload_scene(self.active_scene_uuid);
    }
}
