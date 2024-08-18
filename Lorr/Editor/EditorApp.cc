#include "EditorApp.hh"

#include "Engine/Graphics/Task/BuiltinTasks.hh"

#include "Editor/DummyScene.hh"
#include "Editor/EditorTasks.hh"

namespace lr {
bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    auto transmittance_image = self.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::Storage | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::General,
        .extent = { 256, 64, 1 },
    });

    auto transmittance_image_view = self.device.create_image_view(ImageViewInfo{
        .image_id = transmittance_image,
        .usage_flags = ImageUsage::Storage | ImageUsage::Sampled,
        .type = ImageViewType::View2D,
    });

    auto transmittance_task_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = transmittance_image,
        .image_view_id = transmittance_image_view,
    });

    auto sky_lut_image = self.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::ColorAttachment,
        .extent = { 400, 200, 1 },
        .debug_name = "Sky LUT",
    });
    auto sky_lut_image_view = self.device.create_image_view(ImageViewInfo{
        .image_id = sky_lut_image,
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .type = ImageViewType::View2D,
        .debug_name = "Sky LUT View",
    });
    auto sky_lut_task_image = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = sky_lut_image,
        .image_view_id = sky_lut_image_view,
    });

    auto game_view_image = self.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R32G32B32A32_SFLOAT,
        .type = ImageType::View2D,
        .initial_layout = ImageLayout::ColorAttachment,
        .extent = self.default_surface.swap_chain.extent,
    });
    auto game_view_image_view = self.device.create_image_view(ImageViewInfo{
        .image_id = game_view_image,
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .type = ImageViewType::View2D,
    });
    self.game_view_image_id = self.task_graph.add_image(TaskPersistentImageInfo{
        .image_id = game_view_image,
        .image_view_id = game_view_image_view,
    });

    self.task_graph.add_task<TransmittanceTask>({
        .uses = {
            .storage_image = transmittance_task_image,
        },
    });
    self.task_graph.add_task<SkyLUTTask>({
        .uses = {
            .attachment = sky_lut_task_image,
            .transmittance_lut = transmittance_task_image,
        },
    });
    self.task_graph.add_task<SkyFinalTask>({
        .uses = {
            .attachment = self.game_view_image_id,
            .sky_lut = sky_lut_task_image,
        },
    });

    self.task_graph.add_task<GridTask>({
        .uses = {
            .attachment = self.game_view_image_id,
        },
    });
    self.task_graph.add_task<BuiltinTask::ImGuiTask>({
        .uses = {
            .attachment = self.swap_chain_image_id,
            .font = self.imgui_font_image_id,
        },
    });
    self.task_graph.present(self.swap_chain_image_id);

    self.layout.init();

    auto [dummy_scene_id, dummy_scene] = self.add_scene<DummyScene>("dummy_scene");
    self.set_active_scene(dummy_scene_id);
    dummy_scene->do_load();

    return true;
}

bool EditorApp::update(this EditorApp &self, [[maybe_unused]] f32 delta_time) {
    ZoneScoped;

    self.layout.update();

    return true;
}

}  // namespace lr
