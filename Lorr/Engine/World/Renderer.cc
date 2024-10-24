#include "Engine/World/Renderer.hh"

#include "Engine/Core/Application.hh"
#include "Engine/World/Tasks/ImGui.hh"

#include <glm/gtx/component_wise.hpp>

namespace lr {
WorldRenderer::WorldRenderer(Device device_)
    : device(device_) {
    ZoneScoped;

    this->pbr_graph = TaskGraph::create({
        .device = device_,
        .staging_buffer_size = ls::mib_to_bytes(16),
        .staging_buffer_uses = vk::BufferUsage::Vertex | vk::BufferUsage::Index,
    });
    this->setup_context();
    this->record_pbr_graph(vk::Format::B8G8R8A8_SRGB);
}

auto WorldRenderer::setup_context(this WorldRenderer &self) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    self.context.linear_sampler = Sampler::create(
                                      self.device,
                                      vk::Filtering::Linear,
                                      vk::Filtering::Linear,
                                      vk::Filtering::Linear,
                                      vk::SamplerAddressMode::ClampToEdge,
                                      vk::SamplerAddressMode::ClampToEdge,
                                      vk::SamplerAddressMode::ClampToEdge,
                                      vk::CompareOp::Always)
                                      .value();
    self.context.nearest_sampler = Sampler::create(
                                       self.device,
                                       vk::Filtering::Nearest,
                                       vk::Filtering::Nearest,
                                       vk::Filtering::Nearest,
                                       vk::SamplerAddressMode::ClampToEdge,
                                       vk::SamplerAddressMode::ClampToEdge,
                                       vk::SamplerAddressMode::ClampToEdge,
                                       vk::CompareOp::Always)
                                       .value();

    self.swap_chain_image = self.pbr_graph.add_image({});

    auto [imgui_atlas_data, imgui_atlas_dimensions] = Tasks::imgui_build_font_atlas(asset_man);
    auto imgui_atlas_image = Image::create(
                                 self.device,
                                 vk::ImageUsage::Sampled | vk::ImageUsage::TransferDst,
                                 vk::Format::R8G8B8A8_UNORM,
                                 vk::ImageType::View2D,
                                 vk::Extent3D(imgui_atlas_dimensions.x, imgui_atlas_dimensions.y, 1u))
                                 .value();

    self.device.upload(imgui_atlas_image, imgui_atlas_data, glm::compMul(imgui_atlas_dimensions) * 4, vk::ImageLayout::ColorReadOnly);
    self.imgui_font_image = self.pbr_graph.add_image({
        .image_id = imgui_atlas_image,
        .layout = vk::ImageLayout::ColorReadOnly,
        .access = vk::PipelineAccess::FragmentShaderRead,
    });
}

auto WorldRenderer::record_setup_graph(this WorldRenderer &) -> void {
    ZoneScoped;
}

auto WorldRenderer::record_pbr_graph(this WorldRenderer &self, vk::Format swap_chain_format) -> void {
    ZoneScoped;

    auto &app = Application::get();
    auto &asset_man = app.asset_man;

    self.pbr_graph.add_task(Tasks::ImGuiTask{
        .uses = {
            .attachment = self.swap_chain_image,
        },
        .font_atlas_image = self.imgui_font_image,
        .pipeline = Pipeline::create(self.device, Tasks::imgui_pipeline_info(asset_man, swap_chain_format)).value()
    });
    self.pbr_graph.present(self.swap_chain_image);
}

auto WorldRenderer::render(
    this WorldRenderer &self, SwapChain &swap_chain, ImageID image_id, ls::span<Semaphore> wait_semas, ls::span<Semaphore> signal_semas)
    -> bool {
    ZoneScoped;

    self.pbr_graph.set_image(self.swap_chain_image, { .image_id = image_id });

    self.pbr_graph.execute(TaskExecuteInfo{
        .frame_index = swap_chain.image_index(),
        .execution_data = &self.context,
        .wait_semas = wait_semas,
        .signal_semas = signal_semas,
    });

    return true;
}

}  // namespace lr
