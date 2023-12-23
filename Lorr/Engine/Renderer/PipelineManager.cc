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
    m_pipeline_info.m_dynamic_states.push_back(DynamicState::VK_##state)

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

    if (id + 1 > m_pipeline_info.m_viewports.size())
        m_pipeline_info.m_viewports.resize(id + 1);

    m_pipeline_info.m_viewports[id] = {
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

    if (id + 1 > m_pipeline_info.m_scissors.size())
        m_pipeline_info.m_scissors.resize(id + 1);

    m_pipeline_info.m_scissors[id] = {
        .offset.x = static_cast<i32>(rect.x),
        .offset.y = static_cast<i32>(rect.y),
        .extent.width = rect.z,
        .extent.height = rect.w,
    };
}

void PipelineCompileInfo::set_blend_state(u32 id, const ColorBlendAttachment &state)
{
    ZoneScoped;

    if (id + 1 > m_pipeline_info.m_blend_attachments.size())
        m_pipeline_info.m_blend_attachments.resize(id + 1);
}

void PipelineCompileInfo::set_blend_state_all(const ColorBlendAttachment &state)
{
    ZoneScoped;

    assert(m_pipeline_info.m_blend_attachments.size() != 0);
    for (auto &attachment : m_pipeline_info.m_blend_attachments)
        attachment = state;
}

void PipelineManager::init(Device *device)
{
    m_device = device;

    eastl::vector<DescriptorSetLayout> layouts;
    auto add_layout = [&](DescriptorType type)
    {
        DescriptorLayoutElement elem(0, type, ShaderStage::All, 1);
        layouts.push_back(device->create_descriptor_set_layout(elem, DescriptorSetLayoutFlag::DescriptorBuffer));
    };

    add_layout(DescriptorType::StorageBuffer);
    add_layout(DescriptorType::StorageBuffer);
    add_layout(DescriptorType::Sampler);
    add_layout(DescriptorType::SampledImage);

    PushConstantDesc push_constant_desc(ShaderStage::All, 0, 128);
    m_bindless_layout = m_device->create_pipeline_layout(layouts, push_constant_desc);
}

void PipelineManager::create_pipeline(eastl::string_view name, PipelineCompileInfo &compile_info)
{
    ZoneScoped;

    PipelineInfo pipeline_info = {};

    for (auto &compile_desc : compile_info.m_compile_descs)
    {
        compile_desc.m_include_dirs = compile_info.m_include_dirs;
        auto ir = ShaderCompiler::compile_shader(&compile_desc);

        ShaderReflectionDesc reflection_desc = { .m_reflect_vertex_layout = true };
        auto reflection_data = ShaderCompiler::reflect_spirv(&reflection_desc, ir);
        auto shader_id = m_shaders.size();
        m_device->create_shader(&m_shaders.push_back(), reflection_data.m_compiled_stage, ir);

        pipeline_info.m_bound_shaders.push_back(shader_id);
        pipeline_info.m_reflections.push_back(reflection_data);
    }

    pipeline_info.m_pipeline_id = m_graphics_pipeline_infos.size();
    m_graphics_pipeline_infos.push_back({});

    m_pipeline_infos.emplace(name, pipeline_info);
}

void PipelineManager::compile_pipeline(eastl::string_view name, PipelineAttachmentInfo &attachment_info)
{
    ZoneScoped;

    auto pipeline_info = get_pipeline_info(name);
    if (!pipeline_info)
    {
        LOG_ERROR("Trying to build null pipeline<{}>.", name);
        return;
    }

    assert(pipeline_info->m_pipeline_id == LR_NULL_ID);
    pipeline_info->m_pipeline_id = m_pipelines.size();

    auto &compile_info = m_graphics_pipeline_infos[pipeline_info->m_pipeline_id];
    for (u32 i = 0; i < pipeline_info->m_bound_shaders.size(); i++)
    {
        auto shader_id = pipeline_info->m_bound_shaders[i];
        assert(shader_id != LR_NULL_ID);
        auto &shader = m_shaders[shader_id];
        auto &reflection = pipeline_info->m_reflections[i];

        compile_info.m_vertex_attrib_infos.insert(
            compile_info.m_vertex_attrib_infos.end(), reflection.m_vertex_attribs.begin(), reflection.m_vertex_attribs.end());

        compile_info.m_shader_stages.push_back(PipelineShaderStageInfo(&shader, reflection.m_entry_point));
    }

    if (!compile_info.m_vertex_attrib_infos.empty())
        compile_info.m_vertex_binding_infos.push_back(PipelineVertexLayoutBindingInfo(0, 0, false));

    compile_info.m_layout = m_bindless_layout;

    m_device->create_graphics_pipeline(&m_pipelines.push_back(), &compile_info, &attachment_info);
}

PipelineManager::PipelineInfo *PipelineManager::get_pipeline_info(eastl::string_view name)
{
    ZoneScoped;

    auto iterator = m_pipeline_infos.find(name);
    if (iterator == m_pipeline_infos.end())
        return nullptr;

    return &iterator->second;
}

Pipeline *PipelineManager::get_pipeline(eastl::string_view name)
{
    ZoneScoped;

    auto info = get_pipeline_info(name);
    if (info == nullptr)
        return nullptr;

    return &m_pipelines[info->m_pipeline_id];
}

GraphicsPipelineInfo *PipelineManager::get_graphics_pipeline_info(eastl::string_view name)
{
    ZoneScoped;

    auto info = get_pipeline_info(name);
    if (info == nullptr)
        return nullptr;

    return &m_graphics_pipeline_infos[info->m_pipeline_id];
}
}  // namespace lr::Graphics