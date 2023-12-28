#include "PipelineManager.hh"

#include "Core/FileSystem.hh"

namespace lr::Graphics
{
void PipelineCompileInfo::add_shader(eastl::string_view path)
{
    ZoneScoped;

    ShaderCompileDesc compile_desc = {
        .m_code = fs::read_file(path),
#if _DEBUG
        .m_flags = ShaderCompileFlag::SkipOptimization | ShaderCompileFlag::GenerateDebugInfo,
#endif
    };
    m_compile_descs.push_back(compile_desc);
}

void PipelineCompileInfo::init(usize color_attachment_count)
{
    ZoneScoped;

    m_graphics_pipeline_info.m_blend_attachments.resize(color_attachment_count);
}

void PipelineCompileInfo::add_include_dir(eastl::string_view path)
{
    ZoneScoped;

    m_include_dirs.push_back(eastl::string(path));
}

void PipelineCompileInfo::add_definition(eastl::string_view definition, eastl::string_view equal_to)
{
    ZoneScoped;

    if (equal_to.empty())
        m_definitions.push_back(eastl::string(definition));
    else
        m_definitions.push_back(eastl::format("{}={}", definition, equal_to));
}

void PipelineCompileInfo::set_dynamic_state(DynamicState dynamic_state)
{
    ZoneScoped;

#define ADD_DYNAMIC_STATE(state)             \
    if (dynamic_state & DynamicState::state) \
    m_graphics_pipeline_info.m_dynamic_states.push_back(DynamicState::VK_##state)

    ADD_DYNAMIC_STATE(Viewport);
    ADD_DYNAMIC_STATE(Scissor);
    ADD_DYNAMIC_STATE(ViewportAndCount);
    ADD_DYNAMIC_STATE(ScissorAndCount);
    ADD_DYNAMIC_STATE(DepthTestEnable);
    ADD_DYNAMIC_STATE(DepthWriteEnable);
    ADD_DYNAMIC_STATE(LineWidth);
    ADD_DYNAMIC_STATE(DepthBias);
    ADD_DYNAMIC_STATE(BlendConstants);
}

void PipelineCompileInfo::set_viewport(u32 id, const glm::vec4 &viewport, const glm::vec2 &depth)
{
    ZoneScoped;

    if (id + 1 > m_graphics_pipeline_info.m_viewports.size())
        m_graphics_pipeline_info.m_viewports.resize(id + 1);

    m_graphics_pipeline_info.m_viewports[id] = {
        .x = viewport.x,
        .y = viewport.y,
        .width = viewport.z,
        .height = viewport.w,
        .minDepth = depth.x,
        .maxDepth = depth.y,
    };
}

void PipelineCompileInfo::set_scissors(u32 id, const glm::uvec4 &rect)
{
    ZoneScoped;

    if (id + 1 > m_graphics_pipeline_info.m_scissors.size())
        m_graphics_pipeline_info.m_scissors.resize(id + 1);

    m_graphics_pipeline_info.m_scissors[id] = {
        .offset.x = static_cast<i32>(rect.x),
        .offset.y = static_cast<i32>(rect.y),
        .extent.width = rect.z,
        .extent.height = rect.w,
    };
}

void PipelineCompileInfo::set_blend_state(u32 id, const ColorBlendAttachment &state)
{
    ZoneScoped;

    if (id + 1 > m_graphics_pipeline_info.m_blend_attachments.size())
        m_graphics_pipeline_info.m_blend_attachments.resize(id + 1);
}

void PipelineCompileInfo::set_blend_state_all(const ColorBlendAttachment &state)
{
    ZoneScoped;

    assert(m_graphics_pipeline_info.m_blend_attachments.size() != 0);
    for (auto &attachment : m_graphics_pipeline_info.m_blend_attachments)
        attachment = state;
}

void PipelineManager::init(Device *device)
{
    m_device = device;

    u32 layout_count = 0;
    eastl::array<DescriptorSetLayout, 4> layouts = {};
    auto add_layout = [&](DescriptorType type)
    {
        DescriptorLayoutElement elem(0, type, ShaderStage::All, 1);
        auto &layout = layouts[layout_count++];

        device->create_descriptor_set_layout(&layout, elem, DescriptorSetLayoutFlag::DescriptorBuffer);
    };

    add_layout(DescriptorType::StorageBuffer);
    add_layout(DescriptorType::StorageBuffer);
    add_layout(DescriptorType::Sampler);
    add_layout(DescriptorType::SampledImage);

    PushConstantDesc push_constant_desc(ShaderStage::All, 0, 128);
    m_bindless_layout = m_device->create_pipeline_layout(layouts, push_constant_desc);

    for (auto &layout : layouts)
        m_device->delete_descriptor_set_layout(&layout);
}

PipelineManager::~PipelineManager()
{
    ZoneScoped;

    for (auto &shader : m_shaders.m_resources)
        m_device->delete_shader(&shader);
    for (auto &pipeline : m_pipelines.m_resources)
        m_device->delete_pipeline(&pipeline);
}

PipelineID PipelineManager::compile_pipeline(PipelineCompileInfo &compile_info, PipelineAttachmentInfo &attachment_info)
{
    ZoneScoped;

    auto [pipeline_info_id, pipeline_info] = m_pipeline_infos.add_resource();
    if (!is_handle_valid(pipeline_info_id))
    {
        LOG_ERROR("Cannot compile pipeline info! Resource pool is full.");
        return PipelineID::Invalid;
    }

    auto &graphics_pipeline_info = compile_info.m_graphics_pipeline_info;
    for (auto &compile_desc : compile_info.m_compile_descs)
    {
        compile_desc.m_include_dirs = compile_info.m_include_dirs;
        auto ir = ShaderCompiler::compile_shader(&compile_desc);

        ShaderReflectionDesc reflection_desc = { .m_reflect_vertex_layout = true };
        auto reflection_data = ShaderCompiler::reflect_spirv(&reflection_desc, ir);
        auto [shader_id, shader] = m_shaders.add_resource();
        if (!is_handle_valid(shader_id))
        {
            LOG_ERROR("Cannot create shader! Resource pool is full.");
            return PipelineID::Invalid;
        }

        m_device->create_shader(shader, reflection_data.m_compiled_stage, ir);
        graphics_pipeline_info.m_shader_stages.push_back(PipelineShaderStageInfo(shader, reflection_data.m_entry_point));

        auto &attrib_infos = graphics_pipeline_info.m_vertex_attrib_infos;
        attrib_infos.insert(attrib_infos.end(), reflection_data.m_vertex_attribs.begin(), reflection_data.m_vertex_attribs.end());

        pipeline_info->m_bound_shaders.push_back(shader_id);
        pipeline_info->m_reflections.push_back(reflection_data);
    }

    if (!graphics_pipeline_info.m_vertex_attrib_infos.empty())
        graphics_pipeline_info.m_vertex_binding_infos.push_back(PipelineVertexLayoutBindingInfo(0, 0, false));

    graphics_pipeline_info.m_layout = m_bindless_layout;

    auto [pipeline_id, pipeline] = m_pipelines.add_resource();
    if (!is_handle_valid(pipeline_id))
    {
        LOG_ERROR("Cannot create pipelines. Resource pool is full.");
        return PipelineID::Invalid;
    }

    m_device->create_graphics_pipeline(pipeline, &graphics_pipeline_info, &attachment_info);

    return pipeline_id;
}

PipelineManager::PipelineInfo *PipelineManager::get_pipeline_info(PipelineInfoID pipeline_id)
{
    ZoneScoped;

    return &m_pipeline_infos.get_resource(pipeline_id);
}

Pipeline *PipelineManager::get_pipeline(PipelineID pipeline_id)
{
    ZoneScoped;

    return &m_pipelines.get_resource(pipeline_id);
}

}  // namespace lr::Graphics