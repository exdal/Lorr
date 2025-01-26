#include "Engine/Graphics/Vulkan.hh"

#include "Engine/Graphics/VulkanDevice.hh"

namespace lr {
auto Pipeline::create(Device &device, const ShaderCompileInfo &compile_info) -> std::expected<Pipeline, vuk::VkException> {
    ZoneScoped;

    auto definitions = compile_info.definitions;
    vuk::PipelineBaseCreateInfo create_info = {};

    auto slang_session = device.new_slang_session({
        .definitions = definitions,
        .root_directory = compile_info.root_path,
    });
    auto slang_module = slang_session->load_module({
        .path = compile_info.shader_path,
        .module_name = compile_info.module_name,
        .source = compile_info.shader_source,
    });

    for (auto &v : compile_info.entry_points) {
        auto entry_point = slang_module->get_entry_point(v);
        if (!entry_point.has_value()) {
            LOG_ERROR("Shader stage '{}' is not found for shader module '{}'", v, compile_info.module_name);
            return std::unexpected(VK_ERROR_UNKNOWN);
        }

        auto shader_name = fs::relative(compile_info.shader_path, compile_info.root_path);
        create_info.add_spirv(entry_point->ir, shader_name.string());
    }

    auto *pipeline_handle = device.runtime->get_pipeline(create_info);
    auto pipeline = Pipeline{};
    pipeline.id_ = device.resources.pipelines.create_slot();
    *device.resources.pipelines.slot(pipeline.id_) = pipeline_handle;

    return pipeline;
}

auto Pipeline::id() const -> PipelineID {
    return id_;
}

}  // namespace lr
