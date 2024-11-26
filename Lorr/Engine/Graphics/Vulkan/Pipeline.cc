#include "Engine/Graphics/Vulkan/Impl.hh"

#include "Engine/Graphics/Vulkan/ToVk.hh"

namespace lr {
auto Shader::create(Device_H device, const std::vector<u32> &ir, vk::ShaderStageFlag stage) -> std::expected<Shader, vk::Result> {
    ZoneScoped;

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .codeSize = ir.size() * sizeof(u32),
        .pCode = ir.data(),
    };

    VkShaderModule shader_module = {};
    auto result = vkCreateShaderModule(device->handle, &create_info, nullptr, &shader_module);
    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    auto impl = new Impl;
    impl->device = device;
    impl->stage = stage;
    impl->handle = shader_module;

    return Shader(impl);
}

auto Shader::destroy() -> void {
    vkDestroyShaderModule(impl->device->handle, impl->handle, nullptr);
}

auto Shader::set_name(const std::string &name) -> Shader & {
    set_object_name(impl->device, impl->handle, VK_OBJECT_TYPE_SHADER_MODULE, name);

    return *this;
}

auto Pipeline::create(Device device, const GraphicsPipelineInfo &info) -> std::expected<Pipeline, vk::Result> {
    ZoneScoped;

    std::vector<VkFormat> color_attachment_formats;
    color_attachment_formats.reserve(info.color_attachment_formats.size());
    for (auto &v : info.color_attachment_formats) {
        color_attachment_formats.emplace_back(get_format_info(v).vk_format);
    }

    VkPipelineRenderingCreateInfo rendering_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = nullptr,
        .viewMask = 0,
        .colorAttachmentCount = static_cast<u32>(color_attachment_formats.size()),
        .pColorAttachmentFormats = color_attachment_formats.data(),
        .depthAttachmentFormat = get_format_info(info.depth_attachment_format.value_or(vk::Format::Undefined)).vk_format,
        .stencilAttachmentFormat = get_format_info(info.stencil_attachment_format.value_or(vk::Format::Undefined)).vk_format,
    };

    VkViewport viewport = { .x = 0, .y = 0, .width = 1, .height = 1, .minDepth = 0.0f, .maxDepth = 0.0f };
    VkRect2D scissor = { .offset = { 0, 0 }, .extent = { 1, 1 } };
    VkPipelineViewportStateCreateInfo viewport_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    auto slang_session = device.new_slang_session({
        .definitions = info.shader_module_info.macros,
        .root_directory = info.shader_module_info.root_path,
    });
    auto slang_module = slang_session->load_module({
        .path = info.shader_module_info.shader_path,
        .module_name = info.shader_module_info.module_name,
        .source = info.shader_module_info.shader_source,
    });
    auto module_reflection = slang_module->get_reflection();

    std::vector<Shader> garbage_shaders;
    std::vector<VkPipelineShaderStageCreateInfo> shader_stage_infos;
    shader_stage_infos.reserve(info.shader_module_info.entry_points.size());
    for (auto &v : info.shader_module_info.entry_points) {
        auto entry_point = slang_module->get_entry_point(v);
        if (!entry_point.has_value()) {
            LOG_ERROR("Shader stage '{}' is not found for shader module '{}'", v, info.shader_module_info.module_name);
            return std::unexpected(vk::Result::Unknown);
        }

        auto shader = Shader::create(device, entry_point->ir, entry_point->shader_stage);
        if (!shader.has_value()) {
            LOG_ERROR("Failed to compile shader module '{}', entry point '{}'!", info.shader_module_info.module_name, v);
            return std::unexpected(vk::Result::Unknown);
        }

        shader->set_name(std::format("Shader {}:{}", info.shader_module_info.shader_path, v));

        shader_stage_infos.push_back({
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .stage = to_vk_shader_stage(shader.value()->stage),
            .module = shader.value()->handle,
            .pName = "main",
            .pSpecializationInfo = nullptr,
        });
        garbage_shaders.push_back(shader.value());
    }

    u32 vertex_layout_stride = 0;
    std::vector<VkVertexInputAttributeDescription> input_attrib_descriptions;
    input_attrib_descriptions.reserve(info.vertex_attrib_infos.size());

    for (auto &v : info.vertex_attrib_infos) {
        const auto &format = get_format_info(v.format);
        vertex_layout_stride += format.component_size;
        input_attrib_descriptions.push_back({
            .location = v.location,
            .binding = v.binding,
            .format = format.vk_format,
            .offset = v.offset,
        });
    }

    VkVertexInputBindingDescription input_binding_description = {
        .binding = 0,
        .stride = vertex_layout_stride,
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    };

    VkPipelineVertexInputStateCreateInfo input_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        // Allow layoutless draws, fixes 'VUID-vkCmdDraw-None-04008'
        .vertexBindingDescriptionCount = !info.vertex_attrib_infos.empty(),
        .pVertexBindingDescriptions = &input_binding_description,
        .vertexAttributeDescriptionCount = static_cast<u32>(input_attrib_descriptions.size()),
        .pVertexAttributeDescriptions = input_attrib_descriptions.data(),
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .topology = to_vk_primitive_type(info.rasterizer_state.primitive_type),
        .primitiveRestartEnable = false,
    };

    VkPipelineTessellationStateCreateInfo tessellation_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .patchControlPoints = 0,  // TODO: Tessellation
    };

    VkPipelineRasterizationStateCreateInfo rasterizer_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthClampEnable = info.rasterizer_state.enable_depth_clamp,
        .rasterizerDiscardEnable = false,
        .polygonMode = info.rasterizer_state.enable_wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL,
        .cullMode = to_vk_cull_mode(info.rasterizer_state.cull_mode),
        .frontFace = static_cast<VkFrontFace>(!info.rasterizer_state.front_face_ccw),
        .depthBiasEnable = info.rasterizer_state.enable_depth_bias,
        .depthBiasConstantFactor = info.rasterizer_state.depth_bias_factor,
        .depthBiasClamp = info.rasterizer_state.depth_bias_clamp,
        .depthBiasSlopeFactor = info.rasterizer_state.depth_slope_factor,
        .lineWidth = info.rasterizer_state.line_width,
    };

    VkPipelineMultisampleStateCreateInfo multisample_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .rasterizationSamples = static_cast<VkSampleCountFlagBits>(1 << (info.multisample_state.multisample_bit_count - 1)),
        .sampleShadingEnable = false,
        .minSampleShading = 0,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = info.multisample_state.enable_alpha_to_coverage,
        .alphaToOneEnable = info.multisample_state.enable_alpha_to_one,
    };

    auto &ffo = info.depth_stencil_state.stencil_front_face_op;
    auto &bfo = info.depth_stencil_state.stencil_back_face_op;
    VkStencilOpState ffo_vk = {
        .failOp = to_vk_stencil_op(ffo.fail),
        .passOp = to_vk_stencil_op(ffo.pass),
        .depthFailOp = to_vk_stencil_op(ffo.depth_fail),
        .compareOp = to_vk_compare_op(ffo.compare_func),
        .compareMask = std::to_underlying(ffo.compare_mask),
        .writeMask = std::to_underlying(ffo.write_mask),
        .reference = ffo.reference,
    };
    VkStencilOpState bfo_vk = {
        .failOp = to_vk_stencil_op(bfo.fail),
        .passOp = to_vk_stencil_op(bfo.pass),
        .depthFailOp = to_vk_stencil_op(bfo.depth_fail),
        .compareOp = to_vk_compare_op(bfo.compare_func),
        .compareMask = std::to_underlying(bfo.compare_mask),
        .writeMask = std::to_underlying(bfo.write_mask),
        .reference = bfo.reference,
    };
    VkPipelineDepthStencilStateCreateInfo depth_stencil_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .depthTestEnable = info.depth_stencil_state.enable_depth_test,
        .depthWriteEnable = info.depth_stencil_state.enable_depth_write,
        .depthCompareOp = to_vk_compare_op(info.depth_stencil_state.depth_compare_op),
        .depthBoundsTestEnable = info.depth_stencil_state.enable_depth_bounds_test,
        .stencilTestEnable = info.depth_stencil_state.enable_stencil_test,
        .front = ffo_vk,
        .back = bfo_vk,
        .minDepthBounds = 0,
        .maxDepthBounds = 0,
    };

    std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachment_states;
    color_blend_attachment_states.reserve(info.blend_attachments.size());
    for (auto &v : info.blend_attachments) {
        color_blend_attachment_states.push_back({
            .blendEnable = v.blend_enabled,
            .srcColorBlendFactor = to_vk_blend_factor(v.src_blend),
            .dstColorBlendFactor = to_vk_blend_factor(v.dst_blend),
            .colorBlendOp = to_vk_blend_op(v.blend_op),
            .srcAlphaBlendFactor = to_vk_blend_factor(v.src_blend_alpha),
            .dstAlphaBlendFactor = to_vk_blend_factor(v.dst_blend_alpha),
            .alphaBlendOp = to_vk_blend_op(v.blend_op_alpha),
            .colorWriteMask = static_cast<VkColorComponentFlags>(v.write_mask),
        });
    }

    VkPipelineColorBlendStateCreateInfo color_blend_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .logicOpEnable = false,
        .logicOp = {},
        .attachmentCount = static_cast<u32>(color_blend_attachment_states.size()),
        .pAttachments = color_blend_attachment_states.data(),
        .blendConstants = { info.blend_constants.x, info.blend_constants.y, info.blend_constants.z, info.blend_constants.w },
    };

    constexpr static VkDynamicState DYNAMIC_STATES[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
        VK_DYNAMIC_STATE_DEPTH_BIAS,
    };

    VkPipelineDynamicStateCreateInfo dynamic_state_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .dynamicStateCount = count_of(DYNAMIC_STATES),
        .pDynamicStates = DYNAMIC_STATES,
    };

    VkPipelineLayout pipeline_layout = device->resources.pipeline_layouts.at(module_reflection.pipeline_layout_index);

    VkGraphicsPipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &rendering_info,
        .flags = 0,
        .stageCount = static_cast<u32>(shader_stage_infos.size()),
        .pStages = shader_stage_infos.data(),
        .pVertexInputState = &input_layout_info,
        .pInputAssemblyState = &input_assembly_info,
        .pTessellationState = &tessellation_info,
        .pViewportState = &viewport_state_info,
        .pRasterizationState = &rasterizer_info,
        .pMultisampleState = &multisample_info,
        .pDepthStencilState = &depth_stencil_info,
        .pColorBlendState = &color_blend_info,
        .pDynamicState = &dynamic_state_info,
        .layout = pipeline_layout,
        .renderPass = nullptr,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline_handle = {};
    auto result = vkCreateGraphicsPipelines(device->handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline_handle);

    slang_module->destroy();
    slang_session->destroy();
    for (auto &v : garbage_shaders) {
        v.destroy();
    }

    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    auto pipeline = device->resources.pipelines.create();
    if (!pipeline.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = pipeline->self;
    impl->device = device;
    impl->bind_point = vk::PipelineBindPoint::Graphics;
    impl->layout_id = static_cast<PipelineLayoutID>(module_reflection.pipeline_layout_index);
    impl->id = pipeline->id;
    impl->handle = pipeline_handle;

    auto self = Pipeline(impl);
    self.set_name(std::format("Pipeline {}", info.shader_module_info.module_name));

    return self;
}

auto Pipeline::create(Device device, const ComputePipelineInfo &info) -> std::expected<Pipeline, vk::Result> {
    ZoneScoped;

    auto slang_session = device.new_slang_session({
        .definitions = info.shader_module_info.macros,
        .root_directory = info.shader_module_info.root_path,
    });
    if (!slang_session.has_value()) {
        LOG_ERROR("Failed to initialize compute pipeline session!");
        return std::unexpected(vk::Result::Unknown);
    }

    auto slang_module = slang_session->load_module({
        .path = info.shader_module_info.shader_path,
        .module_name = info.shader_module_info.module_name,
        .source = info.shader_module_info.shader_source,
    });
    if (!slang_module.has_value()) {
        LOG_ERROR("Failed to create compute pipeline module!");
        return std::unexpected(vk::Result::Unknown);
    }

    auto &entry_point_names = info.shader_module_info.entry_points;
    if (entry_point_names.size() != 1) {
        LOG_ERROR("Compute pipeline must have one entry point");
        return std::unexpected(vk::Result::Unknown);
    }

    auto cs_main = slang_module->get_entry_point(info.shader_module_info.entry_points[0]);
    if (!cs_main.has_value()) {
        LOG_ERROR("Compute shader '{}' is missing 'cs_main' entry point!", info.shader_module_info.module_name);
        return std::unexpected(vk::Result::Unknown);
    }
    auto shader = Shader::create(device, cs_main->ir, vk::ShaderStageFlag::Compute);
    auto module_reflection = slang_module->get_reflection();

    VkPipelineShaderStageCreateInfo shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = to_vk_shader_stage(vk::ShaderStageFlag::Compute),
        .module = shader.value()->handle,
        .pName = "main",
        .pSpecializationInfo = nullptr,
    };

    VkPipelineLayout pipeline_layout = device->resources.pipeline_layouts.at(module_reflection.pipeline_layout_index);
    VkComputePipelineCreateInfo pipeline_create_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
        .stage = shader_stage_info,
        .layout = pipeline_layout,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = 0,
    };

    VkPipeline pipeline_handle = {};
    auto result = vkCreateComputePipelines(device->handle, nullptr, 1, &pipeline_create_info, nullptr, &pipeline_handle);

    slang_module->destroy();
    slang_session->destroy();
    shader->destroy();

    switch (result) {
        case VK_ERROR_OUT_OF_HOST_MEMORY:
            return std::unexpected(vk::Result::OutOfHostMem);
        case VK_ERROR_OUT_OF_DEVICE_MEMORY:
            return std::unexpected(vk::Result::OutOfDeviceMem);
        default:;
    }

    auto pipeline = device->resources.pipelines.create();
    if (!pipeline.has_value()) {
        return std::unexpected(vk::Result::OutOfPoolMem);
    }

    auto impl = pipeline->self;
    impl->device = device;
    impl->bind_point = vk::PipelineBindPoint::Compute;
    impl->layout_id = static_cast<PipelineLayoutID>(module_reflection.pipeline_layout_index);
    impl->id = pipeline->id;
    impl->handle = pipeline_handle;

    return Pipeline(impl);
}

auto Pipeline::destroy() -> void {
    vkDestroyPipeline(impl->device->handle, impl->handle, nullptr);
}

auto Pipeline::set_name(const std::string &name) -> Pipeline & {
    set_object_name(impl->device, impl->handle, VK_OBJECT_TYPE_PIPELINE, name);

    return *this;
}

auto Pipeline::bind_point() const -> vk::PipelineBindPoint {
    return impl->bind_point;
}

auto Pipeline::layout_id() const -> PipelineLayoutID {
    return impl->layout_id;
}

auto Pipeline::id() const -> PipelineID {
    return impl->id;
}

}  // namespace lr
