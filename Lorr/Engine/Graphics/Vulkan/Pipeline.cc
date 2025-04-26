#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto Pipeline::create(Device &device, SlangSession &session, const PipelineCompileInfo &compile_info, ls::span<vuk::PersistentDescriptorSet> persistent_sets)
    -> std::expected<Pipeline, vuk::VkException> {
    ZoneScoped;

    vuk::PipelineBaseCreateInfo create_info = {};

    for (const auto &v : persistent_sets) {
        create_info.explicit_set_layouts.push_back(v.set_layout_create_info);
    }

    auto slang_module = session.load_module({
        .module_name = compile_info.module_name,
        .source = compile_info.shader_source,
    });

    for (auto &v : compile_info.entry_points) {
        auto entry_point = slang_module->get_entry_point(v);
        if (!entry_point.has_value()) {
            LOG_ERROR("Shader stage '{}' is not found for shader module '{}'", v, compile_info.module_name);
            return std::unexpected(VK_ERROR_UNKNOWN);
        }

        create_info.add_spirv(entry_point->ir, compile_info.module_name, v);
    }

    auto *pipeline_handle = device.runtime->get_pipeline(create_info);
    auto pipeline = Pipeline {};
    pipeline.id_ = device.resources.pipelines.create_slot();
    *device.resources.pipelines.slot(pipeline.id_) = pipeline_handle;

    return pipeline;
}

auto Pipeline::id() const -> PipelineID {
    return id_;
}

} // namespace lr
