#include "EditorApp.hh"

#include "Engine/Graphics/Task/BuiltinTasks.hh"

#include "Editor/DummyScene.hh"
#include "Editor/EditorTasks.hh"

namespace lr {
struct TransmittanceTask {
    std::string_view name = "Transmittance";

    struct Uses {
        Preset::ComputeWrite storage_image = {};
    } uses = {};

    struct PushConstants {
        glm::vec2 image_size = {};
        ImageViewID image_view_id = ImageViewID::Invalid;
        BufferID atmos_buffer_id = BufferID::Invalid;
    } push_constants = {};

    bool prepare(TaskPrepareInfo &info) {
        auto &app = Application::get();
        auto &pipeline_info = info.pipeline_info;

        auto cs = app.asset_man.shader_at("shader://atmos.transmittance");
        if (!cs) {
            LR_LOG_ERROR("Shaders are invalid.", name);
            return false;
        }

        pipeline_info.set_shader(cs.value());

        return true;
    }

    void execute(TaskContext &tc) {
        auto &storage_image_data = tc.task_image_data(uses.storage_image);
        auto &storage_image = tc.device.image_at(storage_image_data.image_id);

        push_constants.image_size = { storage_image.extent.width, storage_image.extent.height };
        push_constants.image_view_id = storage_image_data.image_view_id;
        tc.set_push_constants(push_constants);

        tc.cmd_list.dispatch(storage_image.extent.width / 16 + 1, storage_image.extent.height / 16 + 1, 1);
    }
};

bool EditorApp::prepare(this EditorApp &self) {
    ZoneScoped;

    auto game_view_image = self.device.create_image(ImageInfo{
        .usage_flags = ImageUsage::ColorAttachment | ImageUsage::Sampled,
        .format = Format::R8G8B8A8_UNORM,
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
