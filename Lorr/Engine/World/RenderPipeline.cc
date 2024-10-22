#include "RenderPipeline.hh"

#include "Engine/World/Components.hh"

#include "Engine/Core/Application.hh"

namespace lr {
bool RenderPipeline::init(this RenderPipeline &self, Device device) {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    self.device = device;
    self.task_graph.init(device);

    // Setup images
    self.swap_chain_image = self.task_graph.add_image({});

    self.linear_sampler = Sampler::create(
                              device,
                              vk::Filtering::Linear,
                              vk::Filtering::Linear,
                              vk::Filtering::Linear,
                              vk::SamplerAddressMode::ClampToEdge,
                              vk::SamplerAddressMode::ClampToEdge,
                              vk::SamplerAddressMode::ClampToEdge,
                              vk::CompareOp::Always)
                              .value();

    self.nearest_sampler = Sampler::create(
                               device,
                               vk::Filtering::Nearest,
                               vk::Filtering::Nearest,
                               vk::Filtering::Nearest,
                               vk::SamplerAddressMode::ClampToEdge,
                               vk::SamplerAddressMode::ClampToEdge,
                               vk::SamplerAddressMode::ClampToEdge,
                               vk::CompareOp::Always)
                               .value();

    self.task_graph.present(self.swap_chain_image);

    return true;
}

void RenderPipeline::shutdown(this RenderPipeline &self) {
    ZoneScoped;
}

void RenderPipeline::update_world_data(this RenderPipeline &self) {
    ZoneScoped;

    auto &app = Application::get();
    auto &world = app.world;
    if (!world.active_scene.has_value()) {
        return;
    }

    auto &scene = world.scene_at(world.active_scene.value());
    auto directional_light_query =  //
        world.ecs
            .query_builder<Component::Transform, Component::DirectionalLight>()  //
            .with(flecs::ChildOf, scene.handle)
            .build();

    directional_light_query.each([&self](flecs::entity, Component::Transform &t, Component::DirectionalLight &l) {
        auto rad = glm::radians(t.rotation);
        glm::vec3 direction = { glm::cos(rad.x) * glm::cos(rad.y), glm::sin(rad.y), glm::sin(rad.x) * glm::cos(rad.y) };
        self.world_data.sun.direction = glm::normalize(direction);
        self.world_data.sun.intensity = l.intensity;
    });
}

bool RenderPipeline::prepare(this RenderPipeline &self, usize frame_index) {
    ZoneScoped;

    return true;
}

bool RenderPipeline::render_into(
    this RenderPipeline &self, SwapChain &swap_chain, ls::span<ImageID> images, ls::span<ImageViewID> image_views) {
    ZoneScoped;

    auto frame_sema = self.device.frame_sema();
    auto present_queue = self.device.queue(vk::CommandType::Graphics);
    usize sema_index = self.device.new_frame();

    auto [acquire_sema, present_sema] = swap_chain.semaphores(sema_index);
    auto acquired_image_index = present_queue.acquire(swap_chain, acquire_sema);
    if (!acquired_image_index.has_value()) {
        return false;
    }

    auto cmd_list = present_queue.begin();
    cmd_list.image_transition({
        .src_stage = vk::PipelineStage::TopOfPipe,
        .src_access = vk::MemoryAccess::None,
        .dst_stage = vk::PipelineStage::BottomOfPipe,
        .dst_access = vk::MemoryAccess::None,
        .old_layout = vk::ImageLayout::Undefined,
        .new_layout = vk::ImageLayout::Present,
        .image_id = images[acquired_image_index.value()],
    });
    present_queue.end(cmd_list);

    Semaphore wait_semas[] = { acquire_sema };
    Semaphore signal_semas[] = { present_sema, frame_sema, present_queue.semaphore(), self.device.transfer_manager().semaphore() };
    present_queue.submit(wait_semas, signal_semas);
    present_queue.present(swap_chain, present_sema, acquired_image_index.value());

    return true;
}

}  // namespace lr
